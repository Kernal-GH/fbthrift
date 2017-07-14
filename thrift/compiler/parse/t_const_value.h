/*
 * Copyright 2004-present Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef T_CONST_VALUE_H
#define T_CONST_VALUE_H

#include <stdint.h>
#include <thrift/compiler/parse/t_const.h>
#include <thrift/compiler/parse/t_enum.h>
#include <thrift/compiler/parse/t_enum_value.h>
#include <thrift/compiler/parse/t_list.h>
#include <thrift/compiler/parse/t_map.h>
#include <thrift/compiler/parse/t_set.h>
#include <thrift/compiler/parse/t_type.h>
#include <utility>
#include <vector>

class t_const;
/**
 * A const value is something parsed that could be a map, set, list, struct
 * or whatever.
 *
 */
class t_const_value {
 public:

  enum t_const_value_type {
    CV_BOOL,
    CV_INTEGER,
    CV_DOUBLE,
    CV_STRING,
    CV_MAP,
    CV_LIST
  };

  t_const_value() {}

  explicit t_const_value(int64_t val) {
    set_integer(val);
  }

  explicit t_const_value(std::string val) {
    set_string(val);
  }

  t_const_value(const t_const_value&) = default;

  void set_string(std::string val) {
    valType_ = CV_STRING;
    stringVal_ = val;
  }

  const std::string& get_string() const {
    return stringVal_;
  }

  void set_integer(int64_t val) {
    valType_ = CV_INTEGER;
    intVal_ = val;
  }

  int64_t get_integer() const {
    return intVal_;
  }

  void set_double(double val) {
    valType_ = CV_DOUBLE;
    doubleVal_ = val;
  }

  double get_double() const {
    return doubleVal_;
  }

  void set_bool(bool val) {
    valType_ = CV_BOOL;
    boolVal_ = val;
    // Added to support backward compatibility with generators that
    // look for the integer value to determine the boolean value
    intVal_ = val;
  }

  bool get_bool() const {
    return boolVal_;
  }

  void set_map() {
    valType_ = CV_MAP;
  }

  void add_map(t_const_value* key, t_const_value* val) {
    mapVal_.emplace_back(key, val);
  }

  const std::vector<std::pair<t_const_value*, t_const_value*>>& get_map()
    const {
    return mapVal_;
  }

  void set_list() {
    valType_ = CV_LIST;
  }

  void add_list(t_const_value* val) {
    listVal_.push_back(val);
  }

  const std::vector<t_const_value*>& get_list() const {
    return listVal_;
  }

  t_const_value_type get_type() const {
    return valType_;
  }

  bool is_empty() const {
    switch (valType_) {
      case CV_MAP:
        return mapVal_.empty();
      case CV_LIST:
        return listVal_.empty();
      case CV_BOOL:
        return boolVal_ == false;
      case CV_INTEGER:
        return intVal_ == 0;
      case CV_DOUBLE:
        return doubleVal_ == 0;
      case CV_STRING:
        return stringVal_.empty();
    }
    return false;
  }

  void set_owner(t_const* owner) {
    owner_ = owner;
  }

  t_const* get_owner() const { return owner_;}

  void set_ttype(t_type* type) {
    type_ = type;
  }

  t_type* get_ttype() const {
    return type_;
  }

  void set_is_enum() {
    is_enum_ = true;
  }

  bool is_enum() const {
    return is_enum_;
  }

  void set_enum(t_enum* tenum) {
    tenum_ = tenum;
  }

  const t_enum* get_enum() const {
    return tenum_;
  }

  void set_enum_value(t_enum_value* tenum_val) {
    tenum_val_ = tenum_val;
  }

  const t_enum_value* get_enum_value() const {
    return tenum_val_;
  }

  static void set_values_type(t_type* type, t_const_value* value) {
    value->set_ttype(type);
    if (type->is_list()) {
      auto* elem_type = dynamic_cast<const t_list*>(type)->get_elem_type();
      for (auto list_val : value->get_list()) {
        set_values_type(elem_type, list_val);
      }
    }
    if (type->is_set()) {
      auto* elem_type = dynamic_cast<const t_set*>(type)->get_elem_type();
      for (auto set_val : value->get_list()) {
        set_values_type(elem_type, set_val);
      }
    }
    if (type->is_map()) {
      auto* key_type = dynamic_cast<const t_map*>(type)->get_key_type();
      auto* val_type = dynamic_cast<const t_map*>(type)->get_val_type();
      for (auto map_val : value->get_map()) {
        set_values_type(key_type, map_val.first);
        set_values_type(val_type, map_val.second);
      }
    }
  }

 private:

  // Use a vector of pairs to store the contents of the map so that we
  // preserve thrift-file ordering when generating per-language source.
  std::vector<std::pair<t_const_value*, t_const_value*>> mapVal_;

  std::vector<t_const_value*> listVal_;
  std::string stringVal_;
  bool boolVal_;
  int64_t intVal_;
  double doubleVal_;

  t_const_value_type valType_;
  t_const* owner_ = nullptr;
  t_type* type_ = nullptr;

  bool is_enum_ = false;
  t_enum* tenum_ = nullptr;
  t_enum_value* tenum_val_ = nullptr;
};

#endif
