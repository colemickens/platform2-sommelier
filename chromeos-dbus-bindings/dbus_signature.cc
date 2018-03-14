// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/dbus_signature.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <brillo/strings/string_utils.h>
#include <dbus/dbus-protocol.h>

using base::StringPrintf;
using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

std::string DBusType::GetOutArgType() const {
  return GetBaseType() + "*";
}

namespace {

// DBusType representing simple numeric types, such as int.
class Scalar : public DBusType {
 public:
  enum class Type {
    kBoolean,
    kByte,
    kDouble,
    kInt16,
    kInt32,
    kInt64,
    kUint16,
    kUint32,
    kUint64,
  };

  explicit Scalar(Type type) : type_(type) {}

  bool IsValidPropertyType() const override { return true; }

  std::string GetBaseType() const override {
    switch (type_) {
      case Type::kBoolean:
        return "bool";
      case Type::kByte:
        return "uint8_t";
      case Type::kDouble:
        return "double";
      case Type::kInt16:
        return "int16_t";
      case Type::kInt32:
        return "int32_t";
      case Type::kInt64:
        return "int64_t";
      case Type::kUint16:
        return "uint16_t";
      case Type::kUint32:
        return "uint32_t";
      case Type::kUint64:
        return "uint64_t";
    }
  }

  std::string GetInArgType() const override { return GetBaseType(); }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(Scalar);
};

// DBusType representing argument types that correspond to C++ objects.
// Example would be brillo::Any or std::map.
class NonScalar : public DBusType {
 public:
  std::string GetInArgType() const override {
    return base::StringPrintf("const %s&", GetBaseType().c_str());
  }
};

class SimpleNonScalar : public NonScalar {
 public:
  enum class Type {
    kFileDescriptor,
    kObjectPath,
    kString,
    kVariant,
    kVariantDict,
  };

  explicit SimpleNonScalar(Type type) : type_(type) {}

  bool IsValidPropertyType() const override {
    // FDs might be lurking inside variants, but they've already been
    // copied into a brillo::Any as the relevant type, so we can't do
    // much about that.
    return type_ != Type::kFileDescriptor;
  }

  std::string GetBaseType() const override {
    switch (type_) {
      case Type::kFileDescriptor:
        return "dbus::FileDescriptor";
      case Type::kObjectPath:
        return "dbus::ObjectPath";
      case Type::kString:
        return "std::string";
      case Type::kVariant:
        return "brillo::Any";
      case Type::kVariantDict:
        return "brillo::VariantDictionary";
    }
  }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(SimpleNonScalar);
};

class Array : public NonScalar {
 public:
  explicit Array(std::unique_ptr<DBusType> inner_type)
      : inner_type_(std::move(inner_type)) {}

  bool IsValidPropertyType() const override {
    return inner_type_->IsValidPropertyType();
  }

  std::string GetBaseType() const override {
    return base::StringPrintf("std::vector<%s>",
                              inner_type_->GetBaseType().c_str());
  }

 private:
  std::unique_ptr<DBusType> inner_type_;

  DISALLOW_COPY_AND_ASSIGN(Array);
};

class Dict : public NonScalar {
 public:
  explicit Dict(std::unique_ptr<DBusType> key_type,
                std::unique_ptr<DBusType> value_type)
      : key_type_(std::move(key_type)), value_type_(std::move(value_type)) {}

  bool IsValidPropertyType() const override {
    return key_type_->IsValidPropertyType() &&
           value_type_->IsValidPropertyType();
  }

  std::string GetBaseType() const override {
    return base::StringPrintf("std::map<%s, %s>",
                              key_type_->GetBaseType().c_str(),
                              value_type_->GetBaseType().c_str());
  }

 private:
  std::unique_ptr<DBusType> key_type_;
  std::unique_ptr<DBusType> value_type_;

  DISALLOW_COPY_AND_ASSIGN(Dict);
};

class Struct : public NonScalar {
 public:
  explicit Struct(std::vector<std::unique_ptr<DBusType>>&& inner_types)
      : inner_types_(std::move(inner_types)) {}

  bool IsValidPropertyType() const override {
    for (const auto& child : inner_types_) {
      if (!child->IsValidPropertyType())
        return false;
    }
    return true;
  }

  std::string GetBaseType() const override {
    std::vector<std::string> child_types;
    for (const auto& child : inner_types_)
      child_types.push_back(child->GetBaseType());
    return base::StringPrintf(
        "std::tuple<%s>",
        brillo::string_utils::Join(", ", child_types).c_str());
  }

 private:
  std::vector<std::unique_ptr<DBusType>> inner_types_;

  DISALLOW_COPY_AND_ASSIGN(Struct);
};

}  // namespace

// static
const char DBusSignature::kArrayTypename[] = "std::vector";
const char DBusSignature::kBooleanTypename[] = "bool";
const char DBusSignature::kByteTypename[] = "uint8_t";
const char DBusSignature::kObjectPathTypename[] = "dbus::ObjectPath";
const char DBusSignature::kDictTypename[] = "std::map";
const char DBusSignature::kDoubleTypename[] = "double";
const char DBusSignature::kSigned16Typename[] = "int16_t";
const char DBusSignature::kSigned32Typename[] = "int32_t";
const char DBusSignature::kSigned64Typename[] = "int64_t";
const char DBusSignature::kStringTypename[] = "std::string";
const char DBusSignature::kUnixFdTypename[] = "dbus::FileDescriptor";
const char DBusSignature::kUnsigned16Typename[] = "uint16_t";
const char DBusSignature::kUnsigned32Typename[] = "uint32_t";
const char DBusSignature::kUnsigned64Typename[] = "uint64_t";
const char DBusSignature::kVariantTypename[] = "brillo::Any";
const char DBusSignature::kVariantDictTypename[] = "brillo::VariantDictionary";
const char DBusSignature::kTupleTypename[] = "std::tuple";

DBusSignature::DBusSignature() = default;

std::unique_ptr<DBusType> DBusSignature::Parse(const string& signature) {
  string::const_iterator end;
  auto type = GetTypenameForSignature(signature.begin(), signature.end(), &end);
  if (!type) {
    LOG(ERROR) << "Parse failed for signature " << signature;
    return nullptr;
  }

  if (end != signature.end()) {
    LOG(WARNING) << "A portion of signature " << signature
                 << " is left unparsed: " << string(end, signature.end());
  }

  return type;
}

bool DBusSignature::Parse(const string& signature, string* output) {
  auto type = Parse(signature);
  if (!type)
    return false;

  *output = type->GetBaseType();
  return true;
}

std::unique_ptr<DBusType> DBusSignature::GetTypenameForSignature(
    string::const_iterator signature,
    string::const_iterator end,
    string::const_iterator* next) {
  DCHECK(next);
  if (signature == end) {
    LOG(ERROR) << "Signature is empty";
    return nullptr;
  }

  string::const_iterator cur = signature;
  int signature_value = *cur++;
  std::unique_ptr<DBusType> type;
  switch (signature_value) {
    case DBUS_STRUCT_BEGIN_CHAR:
      type = GetStructTypenameForSignature(cur, end, &cur);
      break;
    case DBUS_TYPE_ARRAY:
      type = GetArrayTypenameForSignature(cur, end, &cur);
      break;
    case DBUS_TYPE_BOOLEAN:
      type = std::make_unique<Scalar>(Scalar::Type::kBoolean);
      break;
    case DBUS_TYPE_BYTE:
      type = std::make_unique<Scalar>(Scalar::Type::kByte);
      break;
    case DBUS_TYPE_DOUBLE:
      type = std::make_unique<Scalar>(Scalar::Type::kDouble);
      break;
    case DBUS_TYPE_OBJECT_PATH:
      type =
          std::make_unique<SimpleNonScalar>(SimpleNonScalar::Type::kObjectPath);
      break;
    case DBUS_TYPE_INT16:
      type = std::make_unique<Scalar>(Scalar::Type::kInt16);
      break;
    case DBUS_TYPE_INT32:
      type = std::make_unique<Scalar>(Scalar::Type::kInt32);
      break;
    case DBUS_TYPE_INT64:
      type = std::make_unique<Scalar>(Scalar::Type::kInt64);
      break;
    case DBUS_TYPE_STRING:
      type = std::make_unique<SimpleNonScalar>(SimpleNonScalar::Type::kString);
      break;
    case DBUS_TYPE_UNIX_FD:
      type = std::make_unique<SimpleNonScalar>(
          SimpleNonScalar::Type::kFileDescriptor);
      break;
    case DBUS_TYPE_UINT16:
      type = std::make_unique<Scalar>(Scalar::Type::kUint16);
      break;
    case DBUS_TYPE_UINT32:
      type = std::make_unique<Scalar>(Scalar::Type::kUint32);
      break;
    case DBUS_TYPE_UINT64:
      type = std::make_unique<Scalar>(Scalar::Type::kUint64);
      break;
    case DBUS_TYPE_VARIANT:
      type = std::make_unique<SimpleNonScalar>(SimpleNonScalar::Type::kVariant);
      break;
    default:
      LOG(ERROR) << "Unexpected token " << *signature;
      return nullptr;
  }

  *next = cur;
  return type;
}

bool DBusSignature::ParseChildTypes(
    string::const_iterator signature,
    string::const_iterator end,
    string::value_type end_char,
    string::const_iterator* next,
    vector<std::unique_ptr<DBusType>>* children) {
  DCHECK(next);
  DCHECK(children);
  string::const_iterator cur = signature;

  while (cur != end && *cur != end_char) {
    auto child = GetTypenameForSignature(cur, end, &cur);
    if (!child) {
      LOG(ERROR) << "Unable to decode child elements starting at "
                 << string(cur, end);
      return false;
    }

    children->push_back(std::move(child));
  }

  if (cur == end) {
    LOG(ERROR) << "At end of string while processing container type "
               << "starting at " << string(signature, end);
    return false;
  }

  DCHECK_EQ(end_char, *cur);
  *next = cur + 1;
  return true;
}

std::unique_ptr<DBusType> DBusSignature::GetArrayTypenameForSignature(
    string::const_iterator signature,
    string::const_iterator end,
    string::const_iterator* next) {
  DCHECK(next);
  if (signature == end) {
    LOG(ERROR) << "At end of string while reading array parameter";
    return nullptr;
  }

  if (*signature == DBUS_DICT_ENTRY_BEGIN_CHAR)
    return GetDictTypenameForSignature(signature, end, next);

  string::const_iterator cur = signature;
  auto child = GetTypenameForSignature(cur, end, &cur);
  if (!child) {
    LOG(ERROR) << "Unable to decode child element starting at "
               << string(cur, end);
    return nullptr;
  }

  *next = cur;
  return std::make_unique<Array>(std::move(child));
}

std::unique_ptr<DBusType> DBusSignature::GetDictTypenameForSignature(
    string::const_iterator signature,
    string::const_iterator end,
    string::const_iterator* next) {
  DCHECK(next);
  string::const_iterator cur = signature;

  // The dictionary entry type has to be at least 4 characters long:
  // two curly braces and two characters for the key and value types.
  if (end - cur < 4) {
    LOG(ERROR) << "Malformed dictionary at " << string(signature, end);
    return nullptr;
  }

  // Check for VariantDictionary, which is a special case.
  if (string(cur, cur + 4) == "{sv}") {
    *next = cur + 4;
    return std::make_unique<SimpleNonScalar>(
        SimpleNonScalar::Type::kVariantDict);
  }

  ++cur;

  vector<std::unique_ptr<DBusType>> children;
  if (!ParseChildTypes(cur, end, DBUS_DICT_ENTRY_END_CHAR, &cur, &children))
    return nullptr;

  if (children.size() != 2) {
    LOG(ERROR) << "Dict entry contains " << children.size()
               << " members starting at " << string(signature, end)
               << " but dict entries can only have 2 sub-types.";
    return nullptr;
  }

  *next = cur;
  return std::make_unique<Dict>(std::move(children[0]), std::move(children[1]));
}

std::unique_ptr<DBusType> DBusSignature::GetStructTypenameForSignature(
    string::const_iterator signature,
    string::const_iterator end,
    string::const_iterator* next) {
  DCHECK(next);

  if (signature == end) {
    LOG(ERROR) << "At end of string while reading struct parameter";
    return nullptr;
  }

  string::const_iterator cur = signature;
  vector<std::unique_ptr<DBusType>> children;
  if (!ParseChildTypes(cur, end, DBUS_STRUCT_END_CHAR, &cur, &children))
    return nullptr;

  *next = cur;
  return std::make_unique<Struct>(std::move(children));
}

}  // namespace chromeos_dbus_bindings
