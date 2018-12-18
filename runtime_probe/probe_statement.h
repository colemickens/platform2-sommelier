/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef RUNTIME_PROBE_PROBE_STATEMENT_H_
#define RUNTIME_PROBE_PROBE_STATEMENT_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/values.h>

#include "runtime_probe/probe_function.h"

namespace runtime_probe {

class ProbeStatement {
  /* Holds a probe statement with following JSON schema::
   *   {
   *     "eval": <function_name:string> |
   *             <func:ProbeFunction> |
   *             [<func:ProbeFunction>],
   *     "keys": [<key:string>],
   *     "expect": {
   *         <key:string>: <value:string>
   *     },
   *     "information": <info:DictValue>,
   *   }
   *
   * For "eval", the case "[<func:ProbeFunction>]" will be transformed into::
   *   (ProbeFunction) {
   *     "function_name": "sequence",
   *     "args": {
   *       "functions": [<func:ProbeFunction>]
   *     }
   *   }
   */
 public:
  static std::unique_ptr<ProbeStatement> FromDictionaryValue(
      std::string component_name, const base::DictionaryValue& dict_value);

  /* Delegate of eval_->Eval() */
  ProbeFunction::DataType Eval() const;

  std::unique_ptr<base::DictionaryValue> GetInformation() const {
    if (information_ != nullptr)
      return information_->CreateDeepCopy();
    return nullptr;
  }

 private:
  ProbeStatement() = default;

  std::string component_name_;
  std::unique_ptr<ProbeFunction> eval_;
  std::set<std::string> key_;
  std::map<std::string, std::unique_ptr<base::ListValue>> expect_;
  const base::DictionaryValue* information_ = nullptr;
};

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_PROBE_STATEMENT_H_
