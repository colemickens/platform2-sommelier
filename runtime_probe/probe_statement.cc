/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <memory>

#include <base/values.h>

#include "runtime_probe/probe_statement.h"

namespace runtime_probe {

namespace {

void FilterDictionaryValueByKey(base::DictionaryValue* dv,
                                const std::set<std::string>& keys) {
  std::vector<std::string> keys_to_delete;
  for (base::DictionaryValue::Iterator it{*dv}; !it.IsAtEnd(); it.Advance()) {
    if (keys.find(it.key()) == keys.end()) {
      keys_to_delete.push_back(it.key());
    }
  }
  for (const auto& k : keys_to_delete) {
    dv->Remove(k, nullptr);
  }
}

}  // namespace
std::unique_ptr<ProbeStatement> ProbeStatement::FromDictionaryValue(
    std::string component_name, const base::DictionaryValue& dict_value) {
  std::unique_ptr<ProbeStatement> instance{new ProbeStatement};

  instance->component_name_ = component_name;

  // Parse required field "eval"
  const base::Value* eval_value;
  if (!dict_value.Get("eval", &eval_value)) {
    LOG(ERROR) << "eval should be a DictionaryValue: " << *eval_value;
    return nullptr;
  }

  instance->eval_ = ProbeFunction::FromValue(*eval_value);
  // Check the required field eval
  if (!instance->eval_) {
    LOG(ERROR) << "Failed to parse " << dict_value << " as ProbeStatement";
    return nullptr;
  }

  // Parse optional field "keys"
  const base::ListValue* keys_value;
  if (!dict_value.GetList("keys", &keys_value)) {
    VLOG(1) << "keys does not exist or is not a ListValue";
  } else {
    for (auto it = keys_value->begin(); it != keys_value->end(); it++) {
      const auto v = it->get();
      // Currently, destroy all previously inserted valid elems
      if (!v->is_string()) {
        LOG(ERROR) << "keys should be a list of string: " << *keys_value;
        instance->key_.clear();
      }
      instance->key_.insert(v->GetString());
    }
  }

  // Parse optional field "expect"
  // TODO(b:121354690): Make expect useful
  const base::DictionaryValue* expect_dict_value;
  if (!dict_value.GetDictionary("expect", &expect_dict_value)) {
    VLOG(1) << "expect does not exist or is not a DictionaryValue";
  } else {
    for (base::DictionaryValue::Iterator it{*expect_dict_value}; !it.IsAtEnd();
         it.Advance()) {
      // Currently, destroy all previously inserted valid elems
      if (!it.value().is_list()) {
        LOG(ERROR) << "expect should be a DictionaryValue of (string -> list)";
        instance->expect_.clear();
        break;
      }
      instance->expect_[it.key()] =
          base::ListValue::From(std::make_unique<base::Value>(it.value()));
    }
  }
  // Parse optional field "information"
  if (!dict_value.GetDictionary("information", &instance->information_)) {
    VLOG(1) << "information does not exist or is not a DictionaryValue";
  }

  return instance;
}

ProbeFunction::DataType ProbeStatement::Eval() const {
  auto results = eval_->Eval();
  if (!key_.empty()) {
    for (auto& res : results) {
      FilterDictionaryValueByKey(&res, key_);
    }
  }
  return results;
}
}  // namespace runtime_probe
