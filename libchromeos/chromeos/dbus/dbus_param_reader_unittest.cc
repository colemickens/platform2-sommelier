// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/dbus_param_reader.h>

#include <string>

#include <chromeos/any.h>
#include <gtest/gtest.h>

using dbus::MessageReader;
using dbus::MessageWriter;
using dbus::Response;

namespace chromeos {
namespace dbus_utils {

TEST(DBusParamReader, NoArgs) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageReader reader(message.get());
  bool called = false;
  auto callback = [&called]() { called = true; };
  EXPECT_TRUE(DBusParamReader<>::Invoke(callback, &reader, nullptr));
  EXPECT_TRUE(called);
}

TEST(DBusParamReader, OneArg) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  AppendValueToWriter(&writer, 123);
  MessageReader reader(message.get());
  bool called = false;
  auto callback = [&called](int param1) {
    EXPECT_EQ(123, param1);
    called = true;
  };
  EXPECT_TRUE(DBusParamReader<int>::Invoke(callback, &reader, nullptr));
  EXPECT_TRUE(called);
}

TEST(DBusParamReader, ManyArgs) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  AppendValueToWriter(&writer, true);
  AppendValueToWriter(&writer, 1972);
  AppendValueToWriter(&writer, Dictionary{{"key", std::string{"value"}}});
  MessageReader reader(message.get());
  bool called = false;
  auto callback = [&called](bool param1, int param2, const Dictionary& param3) {
    EXPECT_TRUE(param1);
    EXPECT_EQ(1972, param2);
    EXPECT_EQ(1, param3.size());
    EXPECT_EQ("value", param3.find("key")->second.Get<std::string>());
    called = true;
  };
  EXPECT_TRUE((DBusParamReader<bool, int, Dictionary>::Invoke(callback, &reader,
                                                              nullptr)));
  EXPECT_TRUE(called);
}

TEST(DBusParamReader, TooManyArgs) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  AppendValueToWriter(&writer, true);
  AppendValueToWriter(&writer, 1972);
  AppendValueToWriter(&writer, Dictionary{{"key", std::string{"value"}}});
  MessageReader reader(message.get());
  bool called = false;
  auto callback = [&called](bool param1, int param2) {
    EXPECT_TRUE(param1);
    EXPECT_EQ(1972, param2);
    called = true;
  };
  ErrorPtr error;
  EXPECT_FALSE((DBusParamReader<bool, int>::Invoke(callback, &reader, &error)));
  EXPECT_FALSE(called);
  EXPECT_EQ(errors::dbus::kDomain, error->GetDomain());
  EXPECT_EQ(DBUS_ERROR_INVALID_ARGS, error->GetCode());
  EXPECT_EQ("Too many parameters in a method call", error->GetMessage());
}

TEST(DBusParamReader, TooFewArgs) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  AppendValueToWriter(&writer, true);
  MessageReader reader(message.get());
  bool called = false;
  auto callback = [&called](bool param1, int param2) {
    EXPECT_TRUE(param1);
    EXPECT_EQ(1972, param2);
    called = true;
  };
  ErrorPtr error;
  EXPECT_FALSE((DBusParamReader<bool, int>::Invoke(callback, &reader, &error)));
  EXPECT_FALSE(called);
  EXPECT_EQ(errors::dbus::kDomain, error->GetDomain());
  EXPECT_EQ(DBUS_ERROR_INVALID_ARGS, error->GetCode());
  EXPECT_EQ("Too few parameters in a method call", error->GetMessage());
}

TEST(DBusParamReader, TypeMismatch) {
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  MessageWriter writer(message.get());
  AppendValueToWriter(&writer, true);
  AppendValueToWriter(&writer, 1972);
  MessageReader reader(message.get());
  bool called = false;
  auto callback = [&called](bool param1, double param2) {
    EXPECT_TRUE(param1);
    EXPECT_DOUBLE_EQ(1972.0, param2);
    called = true;
  };
  ErrorPtr error;
  EXPECT_FALSE((DBusParamReader<bool, double>::Invoke(callback, &reader,
                                                      &error)));
  EXPECT_FALSE(called);
  EXPECT_EQ(errors::dbus::kDomain, error->GetDomain());
  EXPECT_EQ(DBUS_ERROR_INVALID_ARGS, error->GetCode());
  EXPECT_EQ("Method parameter type mismatch", error->GetMessage());
}

}  // namespace dbus_utils
}  // namespace chromeos
