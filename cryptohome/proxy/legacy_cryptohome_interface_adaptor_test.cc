// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/proxy/legacy_cryptohome_interface_adaptor.h"

#include <attestation-client-test/attestation/dbus-proxy-mocks.h>
#include <chromeos/libhwsec/mock_dbus_method_response.h>
#include <tpm_manager-client-test/tpm_manager/dbus-proxy-mocks.h>

#include "user_data_auth/dbus-proxy-mocks.h"

namespace cryptohome {

namespace {

using ::hwsec::MockDBusMethodResponse;

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::NiceMock;
using ::testing::SaveArg;

// A mock adaptor that is used for testing. This is added so that we can capture
// the Send*() functions used for sending signals.
class LegacyCryptohomeInterfaceAdaptorForTesting
    : public LegacyCryptohomeInterfaceAdaptor {
 public:
  LegacyCryptohomeInterfaceAdaptorForTesting(
      org::chromium::AttestationProxyInterface* attestation_proxy,
      org::chromium::TpmOwnershipProxyInterface* tpm_ownership_proxy,
      org::chromium::TpmNvramProxyInterface* tpm_nvram_proxy,
      org::chromium::UserDataAuthInterfaceProxyInterface* userdataauth_proxy,
      org::chromium::ArcQuotaProxyInterface* arc_quota_proxy,
      org::chromium::CryptohomePkcs11InterfaceProxyInterface* pkcs11_proxy,
      org::chromium::InstallAttributesInterfaceProxyInterface*
          install_attributes_proxy,
      org::chromium::CryptohomeMiscInterfaceProxyInterface* misc_proxy)
      : LegacyCryptohomeInterfaceAdaptor(attestation_proxy,
                                         tpm_ownership_proxy,
                                         tpm_nvram_proxy,
                                         userdataauth_proxy,
                                         arc_quota_proxy,
                                         pkcs11_proxy,
                                         install_attributes_proxy,
                                         misc_proxy) {}

  MOCK_METHOD3(VirtualSendAsyncCallStatusSignal, void(int32_t, bool, int32_t));
  MOCK_METHOD3(VirtualSendAsyncCallStatusWithDataSignal,
               void(int32_t, bool, const std::vector<uint8_t>&));
};

// Some common constants used for testing.
constexpr char kUsername1[] = "foo@gmail.com";
constexpr char kSecret[] = "blah";
constexpr char kSanitizedUsername1[] = "baadf00ddeadbeeffeedcafe";

class LegacyCryptohomeInterfaceAdaptorTest : public ::testing::Test {
 public:
  LegacyCryptohomeInterfaceAdaptorTest() = default;
  ~LegacyCryptohomeInterfaceAdaptorTest() override = default;

  void SetUp() override {
    adaptor_.reset(new LegacyCryptohomeInterfaceAdaptorForTesting(
        &attestation_, &ownership_, &nvram_, &userdataauth_, &arc_quota_,
        &pkcs11_, &install_attributes_, &misc_));

    account_.set_account_id(kUsername1);
    auth_.mutable_key()->set_secret(kSecret);
  }

 protected:
  // Mocks that will be passed into |adaptor_| for its internal use.
  NiceMock<org::chromium::AttestationProxyMock> attestation_;
  NiceMock<org::chromium::TpmOwnershipProxyMock> ownership_;
  NiceMock<org::chromium::TpmNvramProxyMock> nvram_;
  NiceMock<org::chromium::UserDataAuthInterfaceProxyMock> userdataauth_;
  NiceMock<org::chromium::ArcQuotaProxyMock> arc_quota_;
  NiceMock<org::chromium::CryptohomePkcs11InterfaceProxyMock> pkcs11_;
  NiceMock<org::chromium::InstallAttributesInterfaceProxyMock>
      install_attributes_;
  NiceMock<org::chromium::CryptohomeMiscInterfaceProxyMock> misc_;

  // The adaptor that we'll be testing.
  std::unique_ptr<LegacyCryptohomeInterfaceAdaptorForTesting> adaptor_;

  // Default account identifier and authentication request set up with sane
  // value to avoid repeating the same pattern in many test.
  cryptohome::AccountIdentifier account_;
  cryptohome::AuthorizationRequest auth_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyCryptohomeInterfaceAdaptorTest);
};

// -------------------------- MountEx Related Tests --------------------------
TEST_F(LegacyCryptohomeInterfaceAdaptorTest, MountExSuccess) {
  cryptohome::MountRequest req;
  user_data_auth::MountRequest proxied_request;

  req.set_require_ephemeral(false);
  req.set_force_dircrypto_if_available(true);
  req.set_to_migrate_from_ecryptfs(false);
  req.set_public_mount(false);
  req.set_hidden_mount(false);

  EXPECT_CALL(userdataauth_, MountAsync(_, _, _, _))
      .WillOnce(DoAll(
          SaveArg<0>(&proxied_request),
          Invoke([](const user_data_auth::MountRequest& in_request,
                    const base::Callback<void(
                        const user_data_auth::MountReply&)>& success_callback,
                    const base::Callback<void(brillo::Error*)>& error_callback,
                    int timeout_ms) {
            user_data_auth::MountReply proxied_reply;
            proxied_reply.set_recreated(true);
            proxied_reply.set_sanitized_username(kSanitizedUsername1);
            success_callback.Run(proxied_reply);
          })));

  base::Optional<cryptohome::BaseReply> final_reply;
  std::unique_ptr<MockDBusMethodResponse<cryptohome::BaseReply>> response(
      new MockDBusMethodResponse<cryptohome::BaseReply>(nullptr));
  response->save_return_args(&final_reply);

  adaptor_->MountEx(std::move(response), account_, auth_, req);

  // Verify that Return() is indeed called at least once.
  ASSERT_TRUE(final_reply.has_value());

  // Verify its content
  EXPECT_EQ(cryptohome::CRYPTOHOME_ERROR_NOT_SET, final_reply->error());
  EXPECT_TRUE(final_reply->HasExtension(cryptohome::MountReply::reply));
  auto ext = final_reply->GetExtension(cryptohome::MountReply::reply);
  EXPECT_TRUE(ext.recreated());
  EXPECT_EQ(ext.sanitized_username(), kSanitizedUsername1);

  // Verify that the parameters passed to DBus Proxy (New interface) is correct.
  EXPECT_EQ(proxied_request.account().account_id(), kUsername1);
  EXPECT_EQ(proxied_request.authorization().key().secret(), kSecret);
  EXPECT_FALSE(proxied_request.require_ephemeral());
  EXPECT_TRUE(proxied_request.force_dircrypto_if_available());
  EXPECT_FALSE(proxied_request.to_migrate_from_ecryptfs());
  EXPECT_FALSE(proxied_request.public_mount());
  EXPECT_FALSE(proxied_request.hidden_mount());
  EXPECT_FALSE(proxied_request.guest_mount());
}

TEST_F(LegacyCryptohomeInterfaceAdaptorTest, MountExFail) {
  cryptohome::MountRequest req;
  user_data_auth::MountRequest proxied_request;

  req.set_require_ephemeral(true);
  req.set_force_dircrypto_if_available(false);
  req.set_to_migrate_from_ecryptfs(true);
  req.set_public_mount(true);
  req.set_hidden_mount(true);

  EXPECT_CALL(userdataauth_, MountAsync(_, _, _, _))
      .WillOnce(DoAll(
          SaveArg<0>(&proxied_request),
          Invoke([](const user_data_auth::MountRequest& in_request,
                    const base::Callback<void(
                        const user_data_auth::MountReply&)>& success_callback,
                    const base::Callback<void(brillo::Error*)>& error_callback,
                    int timeout_ms) {
            user_data_auth::MountReply proxied_reply;
            proxied_reply.set_error(
                user_data_auth::CRYPTOHOME_ERROR_MOUNT_FATAL);
            proxied_reply.set_recreated(false);
            success_callback.Run(proxied_reply);
          })));

  base::Optional<cryptohome::BaseReply> final_reply;
  std::unique_ptr<MockDBusMethodResponse<cryptohome::BaseReply>> response(
      new MockDBusMethodResponse<cryptohome::BaseReply>(nullptr));
  response->save_return_args(&final_reply);

  adaptor_->MountEx(std::move(response), account_, auth_, req);

  // Verify that Return() is indeed called at least once.
  ASSERT_TRUE(final_reply.has_value());

  // Verify its content
  EXPECT_EQ(cryptohome::CRYPTOHOME_ERROR_MOUNT_FATAL, final_reply->error());
  EXPECT_TRUE(final_reply->HasExtension(cryptohome::MountReply::reply));
  auto ext = final_reply->GetExtension(cryptohome::MountReply::reply);
  EXPECT_FALSE(ext.recreated());

  // Verify that the parameters passed to DBus Proxy (New interface) is correct.
  EXPECT_EQ(proxied_request.account().account_id(), kUsername1);
  EXPECT_EQ(proxied_request.authorization().key().secret(), kSecret);
  EXPECT_TRUE(proxied_request.require_ephemeral());
  EXPECT_FALSE(proxied_request.force_dircrypto_if_available());
  EXPECT_TRUE(proxied_request.to_migrate_from_ecryptfs());
  EXPECT_TRUE(proxied_request.public_mount());
  EXPECT_TRUE(proxied_request.hidden_mount());
  EXPECT_FALSE(proxied_request.guest_mount());
}

}  // namespace

}  // namespace cryptohome
