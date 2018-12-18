/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <memory>
#include <utility>

#include <base/values.h>

#include "runtime_probe/component_category.h"

namespace runtime_probe {

std::unique_ptr<ComponentCategory> ComponentCategory::FromDictionaryValue(
    const std::string& category_name, const base::DictionaryValue& dict_value) {
  std::unique_ptr<ComponentCategory> instance{new ComponentCategory};

  instance->category_name_ = category_name;

  for (base::DictionaryValue::Iterator it{dict_value}; !it.IsAtEnd();
       it.Advance()) {
    const auto component_name = it.key();
    const base::DictionaryValue* probe_statement_dict_value;

    if (!it.value().GetAsDictionary(&probe_statement_dict_value)) {
      LOG(ERROR) << "Component " << component_name
                 << " doesn't map to a DictionaryValue: " << it.value();
      instance = nullptr;
      break;
    }

    instance->component_[component_name] = ProbeStatement::FromDictionaryValue(
        component_name, *probe_statement_dict_value);

    if (!instance->component_[component_name]) {
      instance = nullptr;
      break;
    }
  }

  if (!instance)
    LOG(ERROR) << "Failed to parse " << dict_value << " as ComponentCategory";
  return instance;
}

std::unique_ptr<base::ListValue> ComponentCategory::Eval() const {
  std::unique_ptr<base::ListValue> result{new base::ListValue};

  for (const auto& kv : component_) {
    for (const auto& probe_statement_dv : kv.second->Eval()) {
      std::unique_ptr<base::DictionaryValue> tmp{new base::DictionaryValue};
      tmp->SetString("name", kv.first);
      tmp->Set("values", probe_statement_dv.CreateDeepCopy());
      auto information_dv = kv.second->GetInformation();
      if (information_dv)
        tmp->Set("information", std::move(information_dv));
      result->Append(std::move(tmp));
    }
  }

  return result;
}

}  // namespace runtime_probe
