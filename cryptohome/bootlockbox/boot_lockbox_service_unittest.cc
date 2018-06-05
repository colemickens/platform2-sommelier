// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for BootLockboxDbusAdaptor

#include <utility>

#include <gtest/gtest.h>

#include "cryptohome/bootlockbox/boot_lockbox_dbus_adaptor.h"
#include "cryptohome/bootlockbox/mock_boot_lockbox.h"
#include "cryptohome/mock_crypto.h"
#include "cryptohome/mock_tpm_init.h"

#include "dbus_adaptors/org.chromium.BootLockboxInterface.h"
#include "rpc.pb.h"  // NOLINT(build/include)

using ::testing::_;
using ::testing::NiceMock;

namespace cryptohome {

// DBus Mock
class MockDBusBus : public dbus::Bus {
 public:
  MockDBusBus() : dbus::Bus(dbus::Bus::Options()) {}

  MOCK_METHOD0(Connect, bool());
  MOCK_METHOD0(ShutdownAndBlock, void());
  MOCK_METHOD2(GetServiceOwnerAndBlock,
               std::string(const std::string&,
                           dbus::Bus::GetServiceOwnerOption));
  MOCK_METHOD2(GetObjectProxy,
               dbus::ObjectProxy*(const std::string&, const dbus::ObjectPath&));
};

// Captures the D-Bus Response object passed via DBusMethodResponse via
// ResponseSender.
//
// Example Usage:
//   ResponseCapturer capture;
//   SomeAsyncDBusMethod(capturer.CreateMethodResponse(), ...);
//   EXPECT_EQ(SomeErrorName, capture.response()->GetErrorName());
class ResponseCapturer {
 public:
  ResponseCapturer()
      : call_("org.chromium.BootLockboxInterfaceInterface", "DummyDbusMethod"),
        weak_ptr_factory_(this) {
    call_.SetSerial(1);  // Dummy serial is needed.
  }

  ~ResponseCapturer() = default;

  // Needs to be non-const, because some accessors like GetErrorName() are
  // non-const.
  dbus::Response* response() { return response_.get(); }

  template <typename... Types>
  std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<Types...>>
  CreateMethodResponse() {
    return std::make_unique<brillo::dbus_utils::DBusMethodResponse<Types...>>(
        &call_,
        base::Bind(&ResponseCapturer::Capture, weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void Capture(std::unique_ptr<dbus::Response> response) {
    DCHECK(!response_);
    response_ = std::move(response);
  }

  dbus::MethodCall call_;
  std::unique_ptr<dbus::Response> response_;
  base::WeakPtrFactory<ResponseCapturer> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ResponseCapturer);
};

class BootLockboxDBusAdaptorTest : public ::testing::Test {
 public:
  BootLockboxDBusAdaptorTest() = default;
  ~BootLockboxDBusAdaptorTest() = default;

  void SetUp() override {
    scoped_refptr<MockDBusBus> bus(new MockDBusBus());
    boot_lockbox_dbus_adaptor_.reset(
        new BootLockboxDBusAdaptor(bus, &boot_lockbox_));
  }
 protected:
  NiceMock<MockTpmInit> tpm_init_;
  NiceMock<MockBootLockbox> boot_lockbox_;
  std::unique_ptr<BootLockboxDBusAdaptor> boot_lockbox_dbus_adaptor_;
};

TEST_F(BootLockboxDBusAdaptorTest, SignBootLockbox) {
  cryptohome::SignBootLockboxRequest request;
  request.set_data("test");
  std::vector<uint8_t> request_array(request.ByteSize());
  request.SerializeToArray(request_array.data(), request_array.size());

  EXPECT_CALL(boot_lockbox_, Sign(brillo::BlobFromString("test"), _));
  ResponseCapturer capturer;
  boot_lockbox_dbus_adaptor_->SignBootLockbox(
      capturer.CreateMethodResponse<std::vector<uint8_t>>(),
      request_array);
  dbus::Response* r = capturer.response();
  dbus::MessageReader reader(r);
  cryptohome::BaseReply base_reply;
  reader.PopArrayOfBytesAsProto(&base_reply);
  cryptohome::SignBootLockboxReply signature_reply =
      base_reply.GetExtension(cryptohome::SignBootLockboxReply::reply);
  EXPECT_EQ("", capturer.response()->GetErrorName());
  EXPECT_EQ("", signature_reply.signature());
}

TEST_F(BootLockboxDBusAdaptorTest, VerifyBootLockbox) {
  cryptohome::VerifyBootLockboxRequest request;
  request.set_data("test");
  std::vector<uint8_t> request_array(request.ByteSize());
  request.SerializeToArray(request_array.data(), request_array.size());
  EXPECT_CALL(boot_lockbox_, Verify(brillo::BlobFromString("test"), _));
  ResponseCapturer capturer;
  boot_lockbox_dbus_adaptor_->VerifyBootLockbox(
      capturer.CreateMethodResponse<std::vector<uint8_t>>(),
      request_array);
}

TEST_F(BootLockboxDBusAdaptorTest, FinalizeBootLockbox) {
  cryptohome::FinalizeBootLockboxRequest request;
  std::vector<uint8_t> request_array(request.ByteSize());
  request.SerializeToArray(request_array.data(), request_array.size());
  EXPECT_CALL(boot_lockbox_, FinalizeBoot());
  ResponseCapturer capturer;
  boot_lockbox_dbus_adaptor_->FinalizeBootLockbox(
      capturer.CreateMethodResponse<std::vector<uint8_t>>(),
      request_array);
}

}  // namespace cryptohome
