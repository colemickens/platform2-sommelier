// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "print_tools/ipp_in_json.h"

#include <memory>
#include <utility>

#include <base/json/json_writer.h>
#include <base/macros.h>
#include <base/values.h>

namespace {

std::unique_ptr<base::DictionaryValue> SaveAsJson(ipp::Collection* coll);

// It saves a single value (at given index) from the attribute as JSON
// structure. The parameter "attr" cannot be nullptr, "index" must be correct.
std::unique_ptr<base::Value> SaveAsJson(ipp::Attribute* attr, unsigned index) {
  CHECK(attr != nullptr);
  CHECK(index < attr->GetSize());
  using AttrType = ipp::AttrType;
  switch (attr->GetType()) {
    case AttrType::integer: {
      int vi;
      attr->GetValue(&vi, index);
      return std::make_unique<base::Value>(vi);
    }
    case AttrType::boolean: {
      int vb;
      attr->GetValue(&vb, index);
      return std::make_unique<base::Value>(static_cast<bool>(vb));
    }
    case AttrType::enum_: {
      std::string vs;
      attr->GetValue(&vs, index);
      if (vs.empty()) {
        int vi;
        attr->GetValue(&vi, index);
        return std::make_unique<base::Value>(vi);
      }
      return std::make_unique<base::Value>(vs);
    }
    case AttrType::collection:
      return SaveAsJson(attr->GetCollection(index));
    case AttrType::text:
    case AttrType::name: {
      ipp::StringWithLanguage vs;
      attr->GetValue(&vs, index);
      if (vs.language.empty())
        return std::make_unique<base::Value>(vs.value);
      std::unique_ptr<base::DictionaryValue> obj(new base::DictionaryValue);
      obj->SetString("value", vs.value);
      obj->SetString("language", vs.language);
      return obj;
    }
    case AttrType::dateTime:
    case AttrType::resolution:
    case AttrType::rangeOfInteger:
    case AttrType::octetString:
    case AttrType::keyword:
    case AttrType::uri:
    case AttrType::uriScheme:
    case AttrType::charset:
    case AttrType::naturalLanguage:
    case AttrType::mimeMediaType: {
      std::string vs;
      attr->GetValue(&vs, index);
      return std::make_unique<base::Value>(vs);
    }
  }
  return std::make_unique<base::Value>();  // not reachable
}

// It saves all attribute's values as JSON structure.
// The parameter "attr" cannot be nullptr.
std::unique_ptr<base::Value> SaveAsJson(ipp::Attribute* attr) {
  CHECK(attr != nullptr);
  if (attr->IsASet()) {
    auto arr = std::make_unique<base::ListValue>();
    const unsigned size = attr->GetSize();
    for (unsigned i = 0; i < size; ++i)
      arr->Append(SaveAsJson(attr, i));
    return arr;
  } else {
    return SaveAsJson(attr, 0);
  }
}

// It saves a given Collection as JSON object.
// The parameter "coll" cannot be nullptr.
std::unique_ptr<base::DictionaryValue> SaveAsJson(ipp::Collection* coll) {
  CHECK(coll != nullptr);
  auto obj = std::make_unique<base::DictionaryValue>();
  std::vector<ipp::Attribute*> attrs = coll->GetAllAttributes();

  for (auto a : attrs) {
    auto state = a->GetState();
    if (state == ipp::AttrState::unset)
      continue;

    if (state == ipp::AttrState::set) {
      auto obj2 = std::make_unique<base::DictionaryValue>();
      obj2->SetString("type", ToString(a->GetType()));
      obj2->Set("value", SaveAsJson(a));
      obj->Set(a->GetName(), std::move(obj2));
    } else {
      obj->SetString(a->GetName(), ToString(state));
    }
  }

  return obj;
}

// It saves all groups from given Package as JSON object.
// The parameter "pkg" cannot be nullptr.
std::unique_ptr<base::DictionaryValue> SaveAsJson(ipp::Package* pkg) {
  CHECK(pkg != nullptr);
  auto obj = std::make_unique<base::DictionaryValue>();
  std::vector<ipp::Group*> groups = pkg->GetAllGroups();

  for (auto g : groups) {
    if (g->IsASet()) {
      auto arr = std::make_unique<base::ListValue>();
      const unsigned size = g->GetSize();
      for (unsigned i = 0; i < size; ++i)
        arr->Append(SaveAsJson(g->GetCollection(i)));

      obj->Set(ToString(g->GetName()), std::move(arr));
    } else {
      obj->Set(ToString(g->GetName()), SaveAsJson(g->GetCollection()));
    }
  }

  return obj;
}

// Saves given logs as JSON array.
std::unique_ptr<base::ListValue> SaveAsJson(const std::vector<ipp::Log>& log) {
  auto arr = std::make_unique<base::ListValue>();
  for (const auto& l : log) {
    auto obj = std::make_unique<base::DictionaryValue>();
    obj->SetString("message", l.message);
    if (!l.frame_context.empty())
      obj->SetString("frame_context", l.frame_context);
    if (!l.parser_context.empty())
      obj->SetString("parser_context", l.parser_context);
    arr->Append(std::move(obj));
  }
  return arr;
}

}  // namespace

bool ConvertToJson(ipp::Response& response,  // NOLINT(runtime/references)
                   const std::vector<ipp::Log>& log,
                   bool compressed_json,
                   std::string* json) {
  // Build structure.
  auto doc = std::make_unique<base::DictionaryValue>();
  doc->SetString("status", ipp::ToString(response.StatusCode()));
  if (!log.empty()) {
    doc->Set("parsing_logs", SaveAsJson(log));
  }
  doc->Set("response", SaveAsJson(&response));
  // Convert to JSON.
  bool result;
  if (compressed_json) {
    result = base::JSONWriter::Write(*doc, json);
  } else {
    const int options = base::JSONWriter::OPTIONS_PRETTY_PRINT;
    result = base::JSONWriter::WriteWithOptions(*doc, options, json);
  }
  return result;
}
