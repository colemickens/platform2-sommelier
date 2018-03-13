// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/dbus_signature.h"

#include <map>
#include <string>

#include <dbus/dbus-protocol.h>
#include <gtest/gtest.h>

using std::map;
using std::string;
using testing::Test;

namespace chromeos_dbus_bindings {

namespace {

// Failing signatures.
const char kEmptySignature[] = "";
const char kEmptyDictSignature[] = "a{}";
const char kMissingArraryParameterSignature[] = "a";
const char kMissingArraryParameterInnerSignature[] = "a{sa}i";
const char kOrphanDictSignature[] = "a{s{i}}";
const char kTooFewDictMembersSignature[] = "a{s}";
const char kTooManyDictMembersSignature[] = "a{sa{i}u}";
const char kUnclosedDictOuterSignature[] = "a{s";
const char kUnclosedDictInnerSignature[] = "a{a{u}";
const char kUnexpectedCloseSignature[] = "a}i{";
const char kUnknownSignature[] = "al";

}  // namespace

class DBusSignatureTest : public Test {
 protected:
  DBusSignature signature_;
};

TEST_F(DBusSignatureTest, ParseFailures) {
  for (const auto& failing_string : { kEmptySignature,
                                      kEmptyDictSignature,
                                      kMissingArraryParameterSignature,
                                      kMissingArraryParameterInnerSignature,
                                      kOrphanDictSignature,
                                      kTooFewDictMembersSignature,
                                      kTooManyDictMembersSignature,
                                      kUnclosedDictOuterSignature,
                                      kUnclosedDictInnerSignature,
                                      kUnexpectedCloseSignature,
                                      kUnknownSignature }) {
    string unused_output;
    EXPECT_FALSE(signature_.Parse(failing_string, &unused_output))
        << "Expected signature " << failing_string
        << " to fail but it succeeded";
  }
}

TEST_F(DBusSignatureTest, ParseSuccesses) {
  const map<string, string> parse_values {
    // Simple types.
    { DBUS_TYPE_BOOLEAN_AS_STRING, DBusSignature::kBooleanTypename },
    { DBUS_TYPE_BYTE_AS_STRING, DBusSignature::kByteTypename },
    { DBUS_TYPE_DOUBLE_AS_STRING, DBusSignature::kDoubleTypename },
    { DBUS_TYPE_OBJECT_PATH_AS_STRING, DBusSignature::kObjectPathTypename },
    { DBUS_TYPE_INT16_AS_STRING, DBusSignature::kSigned16Typename },
    { DBUS_TYPE_INT32_AS_STRING, DBusSignature::kSigned32Typename },
    { DBUS_TYPE_INT64_AS_STRING, DBusSignature::kSigned64Typename },
    { DBUS_TYPE_STRING_AS_STRING, DBusSignature::kStringTypename },
    { DBUS_TYPE_UNIX_FD_AS_STRING, DBusSignature::kUnixFdTypename },
    { DBUS_TYPE_UINT16_AS_STRING, DBusSignature::kUnsigned16Typename },
    { DBUS_TYPE_UINT32_AS_STRING, DBusSignature::kUnsigned32Typename },
    { DBUS_TYPE_UINT64_AS_STRING, DBusSignature::kUnsigned64Typename },
    { DBUS_TYPE_VARIANT_AS_STRING, DBusSignature::kVariantTypename },

    // Complex types.
    { "ab",             "std::vector<bool>" },
    { "ay",             "std::vector<uint8_t>" },
    { "aay",            "std::vector<std::vector<uint8_t>>" },
    { "ao",             "std::vector<dbus::ObjectPath>" },
    { "a{oa{sa{sv}}}",  "std::map<dbus::ObjectPath, std::map<std::string, "
                          "brillo::VariantDictionary>>" },
    { "a{os}",          "std::map<dbus::ObjectPath, std::string>" },
    { "as",             "std::vector<std::string>" },
    { "a{ss}",          "std::map<std::string, std::string>" },
    { "a{sa{ss}}",      "std::map<std::string, std::map<std::string, "
                          "std::string>>"},
    { "a{sa{sv}}",      "std::map<std::string, brillo::VariantDictionary>" },
    { "a{sv}",          "brillo::VariantDictionary" },
    { "a{sv}Garbage",   "brillo::VariantDictionary" },
    { "at",             "std::vector<uint64_t>" },
    { "a{iv}",          "std::map<int32_t, brillo::Any>" },
    { "(ib)",           "std::tuple<int32_t, bool>" },
    { "(ibs)",          "std::tuple<int32_t, bool, std::string>" },
  };
  for (const auto& parse_test : parse_values) {
    string output;
    EXPECT_TRUE(signature_.Parse(parse_test.first, &output))
        << "Expected signature " << parse_test.first
        << " to succeed but it failed.";
    EXPECT_EQ(parse_test.second, output)
        << "Expected typename for " << parse_test.first
        << " to be " << parse_test.second << " but instead it was " << output;
  }
}

}  // namespace chromeos_dbus_bindings
