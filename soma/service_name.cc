// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/service_name.h"

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/values.h>

namespace soma {
namespace parser {
namespace service_name {

const char kListKey[] = "annotations";
const char kNameKey[] = "name";
const char kValueKey[] = "value";

bool ParseList(const base::ListValue* annotations,
               std::vector<std::string>* service_names) {
  DCHECK(annotations);
  service_names->resize(annotations->GetSize());
  // TODO(cmasone): Enforce formatting rules on name and value elements.
  for (const base::Value* annotation_value : *annotations) {
    const base::DictionaryValue* annotation = nullptr;
    if (!annotation_value->GetAsDictionary(&annotation)) {
      LOG(ERROR) << "'annotations' must be a list of dicts, not "
                 << annotation_value;
      return false;
    }

    std::string name, value;
    if (!annotation->GetString(kNameKey, &name) ||
        !annotation->GetString(kValueKey, &value)) {
      LOG(ERROR) << "Each annotation must have 'name' and 'value' fields, not "
                 << annotation;
      return false;
    }

    if (StartsWithASCII(name, "service-", false))
      service_names->push_back(value);
    else
      LOG(WARNING) << "Ignore annotation named " << name;
  }
  service_names->shrink_to_fit();
  return true;
}

}  // namespace service_name
}  // namespace parser
}  // namespace soma
