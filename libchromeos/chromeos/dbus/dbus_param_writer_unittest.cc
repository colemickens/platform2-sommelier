// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/dbus_param_writer.h>

#include <string>

#include <chromeos/any.h>
#include <gtest/gtest.h>

using dbus::MessageReader;
using dbus::MessageWriter;
using dbus::ObjectPath;
using dbus::Response;

namespace chromeos {
namespace dbus_utils {

TEST(DBusParamWriter, NoArgs) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  EXPECT_TRUE(DBusParamWriter::Append(&writer));
  EXPECT_EQ("", message->GetSignature());
}

TEST(DBusParamWriter, OneArg) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  EXPECT_TRUE(DBusParamWriter::Append(&writer, int32_t{2}));
  EXPECT_EQ("i", message->GetSignature());
  EXPECT_TRUE(DBusParamWriter::Append(&writer, std::string{"foo"}));
  EXPECT_EQ("is", message->GetSignature());
  EXPECT_TRUE(DBusParamWriter::Append(&writer, ObjectPath{"/o"}));
  EXPECT_EQ("iso", message->GetSignature());

  int32_t int_value = 0;
  std::string string_value;
  ObjectPath path_value;

  MessageReader reader(message.get());
  EXPECT_TRUE(PopValueFromReader(&reader, &int_value));
  EXPECT_TRUE(PopValueFromReader(&reader, &string_value));
  EXPECT_TRUE(PopValueFromReader(&reader, &path_value));

  EXPECT_EQ(2, int_value);
  EXPECT_EQ("foo", string_value);
  EXPECT_EQ(ObjectPath{"/o"}, path_value);
}

TEST(DBusParamWriter, ManyArgs) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  EXPECT_TRUE(DBusParamWriter::Append(&writer, int32_t{9}, Any{7.5}, true));
  EXPECT_EQ("ivb", message->GetSignature());

  int32_t int_value = 0;
  Any variant_value;
  bool bool_value = false;

  MessageReader reader(message.get());
  EXPECT_TRUE(PopValueFromReader(&reader, &int_value));
  EXPECT_TRUE(PopValueFromReader(&reader, &variant_value));
  EXPECT_TRUE(PopValueFromReader(&reader, &bool_value));

  EXPECT_EQ(9, int_value);
  EXPECT_DOUBLE_EQ(7.5, variant_value.Get<double>());
  EXPECT_TRUE(bool_value);
}

TEST(DBusParamWriter, UnsupportedType) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  // 'char' (AKA int8_t) is not supported by D-Bus.
  EXPECT_FALSE(DBusParamWriter::Append(&writer, char{'s'}));
  EXPECT_EQ("", message->GetSignature());
}

}  // namespace dbus_utils
}  // namespace chromeos
