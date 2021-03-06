from libcpp.memory cimport unique_ptr, shared_ptr, make_shared
from libc.string cimport const_uchar
from cython.operator cimport dereference as deref
from libc.stdint cimport uint64_t
from folly.iobuf cimport IOBuf, move
from cpython.ref cimport PyObject
from folly.executor cimport get_executor
from folly.range cimport StringPiece

import asyncio
import collections
import inspect
import ipaddress
from pathlib import Path
import os

from enum import Enum


SocketAddress = collections.namedtuple('SocketAddress', 'ip port path')


cdef inline _get_SocketAddress(const cfollySocketAddress* sadr):
    if sadr.isFamilyInet():
        ip = ipaddress.ip_address(sadr.getAddressStr().decode('utf-8'))
        return SocketAddress(ip=ip, port=sadr.getPort(), path=None)
    return SocketAddress(ip=None, port=None, path=Path(
            os.fsdecode(sadr.getPath())
        )
    )


def pass_context(func):
    """Decorate a handler as wanting the Request Context"""
    func.pass_context = True
    return func


class SSLPolicy(Enum):
    DISABLED = <int> (SSLPolicy__DISABLED)
    PERMITTED = <int> (SSLPolicy__PERMITTED)
    REQUIRED = <int> (SSLPolicy__REQUIRED)


cdef class ServiceInterface:
    pass


cdef void handleAddressCallback(PyObject* future, cfollySocketAddress address):
    (<object>future).set_result(_get_SocketAddress(&address))


cdef class ThriftServer:
    def __cinit__(self):
        self.server = make_shared[cThriftServer]()

    def __init__(self, ServiceInterface handler, int port=0, ip=None, path=None):
        self.loop = asyncio.get_event_loop()
        self.handler = handler

        # Figure out which methods want context and mark them on the handler
        for name, method in inspect.getmembers(handler,
                                               inspect.iscoroutinefunction):
            if hasattr(method, 'pass_context'):
                setattr(handler, f'_pass_context_{name}', True)

        self.server.get().setInterface(handler.interface_wrapper)
        if path:
            fspath = os.fsencode(path)
            self.server.get().setAddress(
                makeFromPath(
                    StringPiece(fspath, len(fspath))
                )
            )
        elif ip:
            # We stringify to accept python ipaddress objects
            self.server.get().setAddress(str(ip).encode('utf-8'), port)
        else:
            self.server.get().setPort(port)
        self.address_future = self.loop.create_future()

    async def serve(self):
        if self.address_future.done():
            self.address_future = self.loop.create_future()
        self.server.get().setServerEventHandler(
            make_shared[Py3ServerEventHandler](
                get_executor(),
                object_partial(handleAddressCallback, <PyObject*> self.address_future)
            )
        )

        def _serve():
            with nogil:
                self.server.get().serve()
        try:
            await self.loop.run_in_executor(None, _serve)
        except Exception as e:
            self.server.get().stop()
            # If somebody is waiting on get_address and the server died
            # then we should forward this exception over to that future. 
            if not self.address_future.done():
                self.address_future.set_exception(e)
            raise

    async def get_address(self):
        return await self.address_future

    def get_active_requests(self):
        return self.server.get().getActiveRequests()

    def get_max_requests(self):
        return self.server.get().getMaxRequests()

    def set_max_requests(self, max_requests):
        self.server.get().setMaxRequests(max_requests)

    def get_max_connections(self):
        return self.server.get().getMaxConnections()

    def set_max_connections(self, max_connections):
        self.server.get().setMaxConnections(max_connections)

    def get_listen_backlog(self):
        return self.server.get().getListenBacklog()

    def set_listen_backlog(self, listen_backlog):
        self.server.get().setListenBacklog(listen_backlog)

    def set_ssl_policy(self, policy):
        cdef cSSLPolicy cPolicy
        if policy == SSLPolicy.DISABLED:
            cPolicy = SSLPolicy__DISABLED
        elif policy == SSLPolicy.PERMITTED:
            cPolicy = SSLPolicy__PERMITTED
        elif policy == SSLPolicy.REQUIRED:
            cPolicy = SSLPolicy__REQUIRED
        self.server.get().setSSLPolicy(cPolicy)

    def stop(self):
        self.server.get().stop()


cdef class ConnectionContext:
    @staticmethod
    cdef ConnectionContext create(Cpp2ConnContext* ctx):
        inst = <ConnectionContext>ConnectionContext.__new__(ConnectionContext)
        inst._ctx = ctx
        inst._peer_address = _get_SocketAddress(ctx.getPeerAddress())
        return inst

    @property
    def peer_address(ConnectionContext self):
        return self._peer_address

    @property
    def peer_common_name(ConnectionContext self):
        return self._ctx.getPeerCommonName().decode('utf-8')

    @property
    def security_protocol(ConnectionContext self):
        return self._ctx.getSecurityProtocol().decode('utf-8')

    @property
    def peer_certificate(ConnectionContext self):
        cdef const_uchar* data
        cdef unique_ptr[IOBuf] der
        cdef shared_ptr[X509] cert
        cdef uint64_t length
        cert = self._ctx.getPeerCertificate()
        if cert.get():
            der = move(derEncode(deref(cert.get())))
            length = der.get().length()
            data = der.get().data()
            return data[:length]
        return None


cdef class RequestContext:
    @staticmethod
    cdef RequestContext create(Cpp2RequestContext* ctx):
        inst = <RequestContext>RequestContext.__new__(RequestContext)
        inst._ctx = ctx
        inst._c_ctx = ConnectionContext.create(ctx.getConnectionContext())
        return inst

    @property
    def connection_context(self):
        return self._c_ctx
