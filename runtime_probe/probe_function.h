/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef RUNTIME_PROBE_PROBE_FUNCTION_H_
#define RUNTIME_PROBE_PROBE_FUNCTION_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <base/json/json_writer.h>
#include <base/values.h>
#include <base/strings/string_util.h>

#include "runtime_probe/probe_function_argument.h"

namespace runtime_probe {

class ProbeFunction {
  /*
   * Formally, a probe function will be represented as following structure::
   *   {
   *     <function_name:string>: <args:ArgsType>
   *   }
   *
   * where the top layer dictionary should have one and only one key.  For
   * example::
   *   {
   *     "shell": {
   *       "command": "mosys platform sku",
   *       "key": "device_sku",
   *       "split_line": false
   *     }
   *   }
   *
   * TODO(stimim): implement the following syntax.
   *
   * Alternative Syntax::
   *   1. single string ("<function_name:string>"), this is equivalent to::
   *      {
   *        <function_name:string>: {}
   *      }
   *
   *   2. single string ("<function_name:string>:<arg:string>"), this is
   *      equivalent to::
   *      {
   *        <function_name:string>: {
   *          "__only_required_argument": {
   *            <arg:string>
   *          }
   *        }
   *      }
   */

 public:
  typedef std::vector<base::DictionaryValue> DataType;

  /* Convert |value| to ProbeFunction.  Returns nullptr on failure.
   */
  static std::unique_ptr<ProbeFunction> FromValue(const base::Value& value) {
    std::unique_ptr<ProbeFunction> retval;

    if (value.is_dict()) {
      const base::DictionaryValue* dict_value;
      value.GetAsDictionary(&dict_value);
      retval = FromDictionaryValue(*dict_value);
    }

    if (retval == nullptr) {
      LOG(ERROR) << "Failed to parse " << value << " as ProbeFunction";
    }
    return retval;
  }

  /* Evaluates this probe function.
   *
   * Output will be a list of base::DictionaryValue.
   */
  virtual DataType Eval() const = 0;

  /* Function prototype of |FromDictionaryValue| that should be implemented by
   * each derived class.  See `functions/shell.h` about how to implement this
   * function.
   */
  typedef std::function<std::unique_ptr<ProbeFunction>(
      const base::DictionaryValue&)>
      FactoryFunctionType;

  /* Mapping from |function_name| to |FromDictionaryValue| of each derived
   * classes.
   */
  static std::unordered_map<std::string_view, FactoryFunctionType>
      REGISTERED_FUNCTIONS;

  template <class T>
  struct Register {
    Register() {
      static_assert(std::is_base_of<ProbeFunction, T>::value,
                    "Registered type must inherit ProbeFunction");
      REGISTERED_FUNCTIONS[T::function_name] = T::FromDictionaryValue;
    }
  };

  virtual ~ProbeFunction() = default;

 protected:
  ProbeFunction() = default;

 private:
  static std::unique_ptr<ProbeFunction> FromDictionaryValue(
      const base::DictionaryValue& dict_value);

  /*
   * Each probe function must define their own args type.
   */
};

#ifdef _RUNTIME_PROBE_GENERATE_PROBE_FUNCTIONS
/* _RUNTIME_PROBE_GENERATE_PROBE_FUNCTIONS should only be defined by
 * `all_functions.cc`.  So the following initialization will only have one
 * instance.
 */

std::unordered_map<std::string_view, ProbeFunction::FactoryFunctionType>
    ProbeFunction::REGISTERED_FUNCTIONS{};
#define REGISTER_PROBE_FUNCTION(T) ProbeFunction::Register<T> T::register_

#else

#define REGISTER_PROBE_FUNCTION(T)

#endif

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_PROBE_FUNCTION_H_
