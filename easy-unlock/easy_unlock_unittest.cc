// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_exported_object.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "easy-unlock/daemon.h"
#include "easy-unlock/fake_easy_unlock_service.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;

namespace {

class MethodCallHandlers {
 public:
  typedef base::Callback<void (dbus::MethodCall* method_call,
                               dbus::ExportedObject::ResponseSender sender)>
      Handler;

  MethodCallHandlers() {}

  ~MethodCallHandlers() {}

  bool SetGenerateEcP256KeyPairHandler(const std::string& interface,
                                       const std::string& method,
                                       Handler handler) {
    generate_ec_p256_key_pair_handler_ = handler;
    return true;
  }

  bool SetPerformECDHKeyAgreementHandler(const std::string& interface,
                                         const std::string& method,
                                         Handler handler) {
    perform_ecdh_key_agreement_handler_ = handler;
    return true;
  }

  bool SetCreateSecureMessageHandler(const std::string& interface,
                                     const std::string& method,
                                     Handler handler) {
    create_secure_message_handler_ = handler;
    return true;
  }

  bool SetUnwrapSecureMessageHandler(const std::string& interface,
                                     const std::string& method,
                                     Handler handler) {
    unwrap_secure_message_handler_ = handler;
    return true;
  }

  void CallGenerateEcP256KeyPair(
      dbus::MethodCall* method_call,
      const dbus::ExportedObject::ResponseSender& sender) {
    ASSERT_FALSE(generate_ec_p256_key_pair_handler_.is_null());
    generate_ec_p256_key_pair_handler_.Run(method_call, sender);
  }

  void CallPerformECDHKeyAgreement(
      dbus::MethodCall* method_call,
      const dbus::ExportedObject::ResponseSender& sender) {
    ASSERT_FALSE(perform_ecdh_key_agreement_handler_.is_null());
    perform_ecdh_key_agreement_handler_.Run(method_call, sender);
  }

  void CallCreateSecureMessage(
      dbus::MethodCall* method_call,
      const dbus::ExportedObject::ResponseSender& sender) {
    ASSERT_FALSE(create_secure_message_handler_.is_null());
    create_secure_message_handler_.Run(method_call, sender);
  }

  void CallUnwrapSecureMessage(
      dbus::MethodCall* method_call,
      const dbus::ExportedObject::ResponseSender& sender) {
    ASSERT_FALSE(unwrap_secure_message_handler_.is_null());
    unwrap_secure_message_handler_.Run(method_call, sender);
  }

 private:
  Handler generate_ec_p256_key_pair_handler_;
  Handler perform_ecdh_key_agreement_handler_;
  Handler create_secure_message_handler_;
  Handler unwrap_secure_message_handler_;

  DISALLOW_COPY_AND_ASSIGN(MethodCallHandlers);
};

class EasyUnlockTest : public ::testing::Test {
 public:
  EasyUnlockTest() : method_call_handlers_(new MethodCallHandlers()) {}
  virtual ~EasyUnlockTest() {}

  virtual void SetUp() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);

    // Create a mock exported object that behaves as
    // org.chromium.EasyUnlock DBus service.
    exported_object_ =
        new dbus::MockExportedObject(
            bus_.get(),
            dbus::ObjectPath(easy_unlock::kEasyUnlockServicePath));
    ASSERT_TRUE(InitializeDaemon());
  }

  virtual void TearDown() {
    if (daemon_)
      daemon_->Finalize();
  }

  void VerifyGenerateEcP256KeyPairResponse(
      scoped_ptr<dbus::Response> response) {
    ASSERT_TRUE(response);

    dbus::MessageReader reader(response.get());

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    ASSERT_TRUE(reader.PopArrayOfBytes(&bytes, &length));
    ASSERT_EQ("private_key_1",
              std::string(reinterpret_cast<const char*>(bytes), length));

    ASSERT_TRUE(reader.PopArrayOfBytes(&bytes, &length));
    ASSERT_EQ("public_key_1",
              std::string(reinterpret_cast<const char*>(bytes), length));
  }

  void VerifyDataResponse(const std::string& expected_content,
                          scoped_ptr<dbus::Response> response) {
    ASSERT_TRUE(response);

    dbus::MessageReader reader(response.get());

    const uint8_t* bytes = nullptr;
    size_t length = 0;

    ASSERT_TRUE(reader.PopArrayOfBytes(&bytes, &length));
    ASSERT_EQ(expected_content,
              std::string(reinterpret_cast<const char*>(bytes), length));
  }

 protected:
  scoped_ptr<MethodCallHandlers> method_call_handlers_;

 private:
  void SetUpExportedObject() {
    EXPECT_CALL(*exported_object_,
                ExportMethodAndBlock(
                    easy_unlock::kEasyUnlockServiceInterface,
                    easy_unlock::kGenerateEcP256KeyPairMethod,
                    _))
        .WillOnce(
             Invoke(method_call_handlers_.get(),
                    &MethodCallHandlers::SetGenerateEcP256KeyPairHandler));
    EXPECT_CALL(*exported_object_,
                ExportMethodAndBlock(
                    easy_unlock::kEasyUnlockServiceInterface,
                    easy_unlock::kPerformECDHKeyAgreementMethod,
                    _))
        .WillOnce(
             Invoke(method_call_handlers_.get(),
                    &MethodCallHandlers::SetPerformECDHKeyAgreementHandler));
    EXPECT_CALL(*exported_object_,
                ExportMethodAndBlock(
                    easy_unlock::kEasyUnlockServiceInterface,
                    easy_unlock::kCreateSecureMessageMethod,
                    _))
        .WillOnce(
             Invoke(method_call_handlers_.get(),
                    &MethodCallHandlers::SetCreateSecureMessageHandler));
    EXPECT_CALL(*exported_object_,
                ExportMethodAndBlock(
                    easy_unlock::kEasyUnlockServiceInterface,
                    easy_unlock::kUnwrapSecureMessageMethod,
                    _))
        .WillOnce(
             Invoke(method_call_handlers_.get(),
                    &MethodCallHandlers::SetUnwrapSecureMessageHandler));

    EXPECT_CALL(*exported_object_,
                ExportMethodAndBlock(
                    "org.freedesktop.DBus.Introspectable",
                    "Introspect",
                    _)).WillOnce(Return(true));
  }

  bool InitializeDaemon() {
    EXPECT_CALL(*bus_, Connect()).WillOnce(Return(true));
    EXPECT_CALL(*bus_, SetUpAsyncOperations()).WillOnce(Return(true));
    EXPECT_CALL(*bus_,
        RequestOwnershipAndBlock(easy_unlock::kEasyUnlockServiceName,
                                 dbus::Bus::REQUIRE_PRIMARY))
            .WillOnce(Return(true));
    // Should get called on during daemon shutdown.
    EXPECT_CALL(*bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // |bus_|'s GetExportedObject() will return |exported_object_|
    // for the given service name and the object path.
    EXPECT_CALL(*bus_.get(),
                GetExportedObject(dbus::ObjectPath(
                    easy_unlock::kEasyUnlockServicePath)))
        .WillOnce(Return(exported_object_.get()));

    SetUpExportedObject();

    scoped_ptr<easy_unlock::FakeService> fake_service(
        new easy_unlock::FakeService());
    daemon_.reset(
        new easy_unlock::Daemon(
            scoped_ptr<easy_unlock::Service>(new easy_unlock::FakeService()),
            bus_,
            base::Closure(),
            false));
    return daemon_->Initialize();
  }

  base::MessageLoopForIO message_loop_;

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockExportedObject> exported_object_;

  scoped_ptr<easy_unlock::Daemon> daemon_;
};

TEST_F(EasyUnlockTest, GenerateEcP256KeyPair) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kGenerateEcP256KeyPairMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);
  method_call_handlers_->CallGenerateEcP256KeyPair(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyGenerateEcP256KeyPairResponse,
                 base::Unretained(this)));
}

TEST_F(EasyUnlockTest, PerformECDHKeyAgreement) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kPerformECDHKeyAgreementMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string private_key = "private_key_1";
  const std::string public_key = "public_key_2";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(private_key.data()),
      private_key.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(public_key.data()),
                            public_key.length());

  method_call_handlers_->CallPerformECDHKeyAgreement(
      &method_call,
      base::Bind(
          &EasyUnlockTest::VerifyDataResponse,
          base::Unretained(this),
          "secret_key:{private_key:private_key_1,public_key:public_key_2}"));
}

TEST_F(EasyUnlockTest, CreateSecureMessage) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kCreateSecureMessageMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string payload = "cleartext message";
  const std::string key = "secret key";
  const std::string associated_data = "ad";
  const std::string public_metadata = "pm";
  const std::string verification_key_id = "key";
  const std::string decryption_key_id = "key1";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(payload.data()),
                            payload.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(key.data()),
                            key.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(public_metadata.data()),
      public_metadata.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(verification_key_id.data()),
      verification_key_id.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(decryption_key_id.data()),
      decryption_key_id.length());
  writer.AppendString(easy_unlock::kEncryptionTypeAES256CBC);
  writer.AppendString(easy_unlock::kSignatureTypeHMACSHA256);

  const std::string expected_response =
    "securemessage:{"
      "payload:cleartext message,"
      "key:secret key,"
      "associated_data:ad,"
      "public_metadata:pm,"
      "verification_key_id:key,"
      "decryption_key_id:key1,"
      "encryption:AES,"
      "signature:HMAC"
    "}";

  method_call_handlers_->CallCreateSecureMessage(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyDataResponse,
                 base::Unretained(this),
                 expected_response));
}

TEST_F(EasyUnlockTest, CreateSecureMessage_NoDecryptionKeyId) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kCreateSecureMessageMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string payload = "cleartext message";
  const std::string key = "secret key";
  const std::string associated_data = "ad";
  const std::string public_metadata = "pm";
  const std::string verification_key_id = "key";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(payload.data()),
                            payload.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(key.data()),
                            key.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(public_metadata.data()),
      public_metadata.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(verification_key_id.data()),
      verification_key_id.length());
  writer.AppendString(easy_unlock::kEncryptionTypeAES256CBC);
  writer.AppendString(easy_unlock::kSignatureTypeHMACSHA256);

  const std::string expected_response =
    "securemessage:{"
      "payload:cleartext message,"
      "key:secret key,"
      "associated_data:ad,"
      "public_metadata:pm,"
      "verification_key_id:key,"
      "decryption_key_id:,"
      "encryption:AES,"
      "signature:HMAC"
    "}";

  method_call_handlers_->CallCreateSecureMessage(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyDataResponse,
                 base::Unretained(this),
                 expected_response));
}

TEST_F(EasyUnlockTest, CreateSecureMessage_Invalid_MissingParameter) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kCreateSecureMessageMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string payload = "cleartext message";
  const std::string key = "secret key";
  const std::string associated_data = "ad";
  const std::string verification_key_id = "key";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(payload.data()),
                            payload.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(key.data()),
                            key.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(verification_key_id.data()),
      verification_key_id.length());
  writer.AppendString(easy_unlock::kEncryptionTypeAES256CBC);
  writer.AppendString(easy_unlock::kSignatureTypeHMACSHA256);

  method_call_handlers_->CallCreateSecureMessage(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyDataResponse,
                 base::Unretained(this),
                 ""));
}

TEST_F(EasyUnlockTest, CreateSecureMessage_Invalid_UnknownEncryptionType) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kCreateSecureMessageMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string payload = "cleartext message";
  const std::string key = "secret key";
  const std::string associated_data = "ad";
  const std::string public_metadata = "pm";
  const std::string verification_key_id = "key";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(payload.data()),
                            payload.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(key.data()),
                            key.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(public_metadata.data()),
      public_metadata.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(verification_key_id.data()),
      verification_key_id.length());
  writer.AppendString("UNKNOWN");
  writer.AppendString(easy_unlock::kSignatureTypeHMACSHA256);

  method_call_handlers_->CallCreateSecureMessage(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyDataResponse,
                 base::Unretained(this),
                 ""));
}

TEST_F(EasyUnlockTest, CreateSecureMessage_Invalid_UnknownSignatureType) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kCreateSecureMessageMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string payload = "cleartext message";
  const std::string key = "secret key";
  const std::string associated_data = "ad";
  const std::string public_metadata = "pm";
  const std::string verification_key_id = "key";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(payload.data()),
                            payload.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(key.data()),
                            key.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(public_metadata.data()),
      public_metadata.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(verification_key_id.data()),
      verification_key_id.length());
  writer.AppendString(easy_unlock::kEncryptionTypeAES256CBC);
  writer.AppendString("UNKOWN");

  method_call_handlers_->CallCreateSecureMessage(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyDataResponse,
                 base::Unretained(this),
                 ""));
}

TEST_F(EasyUnlockTest, UnwrapSecureMessage) {
  dbus::MethodCall method_call(easy_unlock::kEasyUnlockServiceInterface,
                               easy_unlock::kUnwrapSecureMessageMethod);
  // Set serial to an arbitrary value.
  method_call.SetSerial(231);

  const std::string message = "secure message";
  const std::string key = "secret key";
  const std::string associated_data = "ad";

  dbus::MessageWriter writer(&method_call);
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(message.data()),
                            message.length());
  writer.AppendArrayOfBytes(reinterpret_cast<const uint8_t*>(key.data()),
                            key.length());
  writer.AppendArrayOfBytes(
      reinterpret_cast<const uint8_t*>(associated_data.data()),
      associated_data.length());
  writer.AppendString(easy_unlock::kEncryptionTypeAES256CBC);
  writer.AppendString(easy_unlock::kSignatureTypeHMACSHA256);

  const std::string expected_response =
    "unwrappedmessage:{"
      "original:secure message,"
      "key:secret key,"
      "associated_data:ad,"
      "encryption:AES,"
      "signature:HMAC"
    "}";

  method_call_handlers_->CallUnwrapSecureMessage(
      &method_call,
      base::Bind(&EasyUnlockTest::VerifyDataResponse,
                 base::Unretained(this),
                 expected_response));
}

}  // namespace
