// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/annotations.h"

#include <memory>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/values.h>

namespace soma {
namespace parser {
namespace annotations {

const char kListKey[] = "annotations";
const char kPersistentKey[] = "bruteus-persistent";

namespace {

const char kServiceNameKeyPrefix[] = "bruteus-service";
const char kNameKey[] = "name";
const char kValueKey[] = "value";

bool ParseAnnotation(const base::Value& annotation_value,
                     std::string* name, std::string* value) {
  DCHECK(name);
  DCHECK(value);
  // TODO(cmasone): Enforce formatting rules on name and value elements.
  const base::DictionaryValue* annotation = nullptr;
  if (!annotation_value.GetAsDictionary(&annotation)) {
    LOG(ERROR) << "'annotations' must be a list of dicts, not "
               << annotation_value;
    return false;
  }
  if (!annotation->GetString(kNameKey, name) ||
      !annotation->GetString(kValueKey, value)) {
    LOG(ERROR) << "Each annotation must have 'name' and 'value' fields, not "
               << annotation;
    return false;
  }
  return true;
}

}  // namespace

std::string MakeServiceNameKey(size_t index) {
  return base::StringPrintf("%s-%zu", kServiceNameKeyPrefix, index);
}

bool ParseServiceNameList(const base::ListValue& annotations,
                          std::vector<std::string>* service_names) {
  service_names->resize(annotations.GetSize());
  for (const base::Value* annotation_value : annotations) {
    std::string name, value;
    if (!annotations::ParseAnnotation(*annotation_value, &name, &value))
      return false;
    if (StartsWithASCII(name, kServiceNameKeyPrefix, false))
      service_names->push_back(value);
  }
  service_names->shrink_to_fit();
  return true;
}

bool IsPersistent(const base::ListValue& annotations) {
  for (const base::Value* annotation_value : annotations) {
    std::string name, value;
    if (!annotations::ParseAnnotation(*annotation_value, &name, &value))
      return false;
    if (name == kPersistentKey)
      return LowerCaseEqualsASCII(value, "true");
  }
  return false;
}

namespace {
std::unique_ptr<base::DictionaryValue> CreateAnnotation(
    const std::string& name,
    const std::string& value) {
  std::unique_ptr<base::DictionaryValue> annotation(new base::DictionaryValue);
  annotation->SetString(kNameKey, name);
  annotation->SetString(kValueKey, value);
  return std::move(annotation);
}
}  // namespace

bool AddPersistentAnnotationForTest(base::DictionaryValue* to_modify) {
  DCHECK(to_modify);
  base::ListValue* annotations = nullptr;
  if (!to_modify->GetList(kListKey, &annotations))
    return false;
  annotations->Append(CreateAnnotation(kPersistentKey, "true").release());
  return true;
}

}  // namespace annotations
}  // namespace parser
}  // namespace soma
