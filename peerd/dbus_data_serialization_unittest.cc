// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/dbus_data_serialization.h"

#include <algorithm>

#include <gtest/gtest.h>

#include "peerd/ip_addr.h"

using dbus::Response;

namespace peerd {

TEST(DBusDataSerialization, Signature_sockaddr) {
  EXPECT_EQ("(ayq)",
            chromeos::dbus_utils::GetDBusSignature<ip_addr>());
}

TEST(DBusDataSerialization, Write_sockaddr_in) {
  ip_addr addr;
  addr.ss_family = AF_INET;
  sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(&addr);
  addr_in->sin_port = 1234;
  addr_in->sin_addr.s_addr = 0x12345678;

  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  dbus::MessageWriter writer(message.get());
  chromeos::dbus_utils::AppendValueToWriter(&writer, addr);

  EXPECT_EQ("(ayq)", message->GetSignature());

  dbus::MessageReader reader(message.get());
  dbus::MessageReader struct_reader(nullptr);
  ASSERT_TRUE(reader.PopStruct(&struct_reader));

  const uint8_t* addr_buffer = nullptr;
  size_t addr_len = 0;
  ASSERT_TRUE(struct_reader.PopArrayOfBytes(&addr_buffer, &addr_len));
  EXPECT_EQ(4, addr_len);
  EXPECT_EQ(0x78, addr_buffer[0]);
  EXPECT_EQ(0x56, addr_buffer[1]);
  EXPECT_EQ(0x34, addr_buffer[2]);
  EXPECT_EQ(0x12, addr_buffer[3]);

  uint16_t port = 0;
  ASSERT_TRUE(struct_reader.PopUint16(&port));
  EXPECT_EQ(1234, port);
}

TEST(DBusDataSerialization, Read_sockaddr_in) {
  uint8_t addr_buffer[4] = {0x12, 0x34, 0x56, 0x78};
  static_assert(arraysize(addr_buffer) == sizeof(sockaddr_in::sin_addr),
                "Unexpected size of address buffer");

  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  dbus::MessageWriter writer(message.get());
  dbus::MessageWriter struct_writer(nullptr);
  writer.OpenStruct(&struct_writer);
  struct_writer.AppendArrayOfBytes(addr_buffer, arraysize(addr_buffer));
  struct_writer.AppendUint16(80);
  writer.CloseContainer(&struct_writer);

  EXPECT_EQ("(ayq)", message->GetSignature());

  dbus::MessageReader reader(message.get());
  ip_addr addr;
  ASSERT_TRUE(chromeos::dbus_utils::PopValueFromReader(&reader, &addr));

  EXPECT_EQ(AF_INET, addr.ss_family);
  sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(&addr);
  EXPECT_EQ(0x78563412, addr_in->sin_addr.s_addr);
  EXPECT_EQ(80, addr_in->sin_port);
}

TEST(DBusDataSerialization, Write_sockaddr_in6) {
  uint8_t addr_buffer[16] = {
      0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
  };
  static_assert(arraysize(addr_buffer) == sizeof(sockaddr_in6::sin6_addr),
                "Unexpected size of address buffer");
  ip_addr addr;
  addr.ss_family = AF_INET6;
  sockaddr_in6* addr_in6 = reinterpret_cast<sockaddr_in6*>(&addr);
  addr_in6->sin6_port = 1234;
  std::copy(std::begin(addr_buffer), std::end(addr_buffer),
            reinterpret_cast<uint8_t*>(&addr_in6->sin6_addr));

  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  dbus::MessageWriter writer(message.get());
  chromeos::dbus_utils::AppendValueToWriter(&writer, addr);

  EXPECT_EQ("(ayq)", message->GetSignature());

  dbus::MessageReader reader(message.get());
  dbus::MessageReader struct_reader(nullptr);
  ASSERT_TRUE(reader.PopStruct(&struct_reader));

  const uint8_t* out_addr_buffer = nullptr;
  size_t out_addr_len = 0;
  ASSERT_TRUE(struct_reader.PopArrayOfBytes(&out_addr_buffer, &out_addr_len));
  EXPECT_EQ(arraysize(addr_buffer), out_addr_len);
  EXPECT_EQ(0, memcmp(addr_buffer, out_addr_buffer, out_addr_len));

  uint16_t port = 0;
  ASSERT_TRUE(struct_reader.PopUint16(&port));
  EXPECT_EQ(1234, port);
}

TEST(DBusDataSerialization, Read_sockaddr_in6) {
  uint8_t addr_buffer[16] = {
      0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
  };
  static_assert(arraysize(addr_buffer) == sizeof(sockaddr_in6::sin6_addr),
                "Unexpected size of address buffer");
  std::unique_ptr<Response> message(Response::CreateEmpty().release());
  dbus::MessageWriter writer(message.get());
  dbus::MessageWriter struct_writer(nullptr);
  writer.OpenStruct(&struct_writer);
  struct_writer.AppendArrayOfBytes(addr_buffer, arraysize(addr_buffer));
  struct_writer.AppendUint16(8080);
  writer.CloseContainer(&struct_writer);

  EXPECT_EQ("(ayq)", message->GetSignature());

  dbus::MessageReader reader(message.get());
  ip_addr addr;
  ASSERT_TRUE(chromeos::dbus_utils::PopValueFromReader(&reader, &addr));

  EXPECT_EQ(AF_INET6, addr.ss_family);
  sockaddr_in6* addr_in6 = reinterpret_cast<sockaddr_in6*>(&addr);
  EXPECT_EQ(0,
            memcmp(addr_buffer, &addr_in6->sin6_addr, arraysize(addr_buffer)));
  EXPECT_EQ(8080, addr_in6->sin6_port);
}
}  // namespace peerd
