// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// trunks_client is a command line tool that supports various TPM operations. It
// does not provide direct access to the trunksd D-Bus interface.

#include <stdio.h>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <chromeos/syslog_logging.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>

#include "trunks/error_codes.h"
#include "trunks/password_authorization_delegate.h"
#include "trunks/scoped_key_handle.h"
#include "trunks/tpm_state.h"
#include "trunks/tpm_utility.h"
#include "trunks/trunks_factory_impl.h"

namespace {

void PrintUsage() {
  puts("Options:");
  puts("  --help - Prints this message.");
  puts("  --status - Prints TPM status information.");
  puts("  --startup - Performs startup and self-tests.");
  puts("  --clear - Clears the TPM. Use before initializing the TPM.");
  puts("  --init_tpm - Initializes a TPM as CrOS firmware does.");
  puts("  --own - Takes ownership of the TPM with the provided password.");
  puts("  --regression_test - Runs some basic regression tests. If");
  puts("                      owner_password is supplied, it runs tests that");
  puts("                      need owner permissions.");
  puts("  --owner_password - used to provide an owner password");
}

int Startup() {
  trunks::TrunksFactoryImpl factory;
  factory.GetTpmUtility()->Shutdown();
  return factory.GetTpmUtility()->Startup();
}

int Clear() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->Clear();
}

int InitializeTpm() {
  trunks::TrunksFactoryImpl factory;
  return factory.GetTpmUtility()->InitializeTpm();
}

int TakeOwnership(const std::string& owner_password) {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_RC rc;
  rc = factory.GetTpmUtility()->TakeOwnership(owner_password,
                                              owner_password,
                                              owner_password);
  if (rc) {
    LOG(ERROR) << "Error taking ownership: " << trunks::GetErrorString(rc);
    return rc;
  }
  return 0;
}

int RNGTest() {
  trunks::TrunksFactoryImpl factory;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  scoped_ptr<trunks::AuthorizationSession> session =
      factory.GetAuthorizationSession();
  std::string entropy_data("entropy_data");
  std::string random_data;
  size_t num_bytes = 70;
  trunks::TPM_RC rc;
  rc = session->StartUnboundSession(true /* enable encryption */);
  if (rc) {
    LOG(ERROR) << "Error starting authorization session: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  session->SetEntityAuthorizationValue("");
  rc = utility->StirRandom(entropy_data, session.get());
  if (rc) {
    LOG(ERROR) << "Error stirring TPM random number generator: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  session->SetEntityAuthorizationValue("");
  rc = utility->GenerateRandom(num_bytes, session.get(), &random_data);
  if (rc) {
    LOG(ERROR) << "Error getting random bytes from TPM: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  if (num_bytes != random_data.size()) {
    LOG(ERROR) << "Error not enough random bytes received.";
    return -1;
  }
  LOG(INFO) << "Test completed successfully.";
  return 0;
}

int SignTest() {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_HANDLE signing_key;
  trunks::TPM_RC rc;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  scoped_ptr<trunks::AuthorizationSession> session(
      factory.GetAuthorizationSession());
  rc = session->StartUnboundSession(true /* enable encryption */);
  if (rc) {
    LOG(ERROR) << "Error starting authorization session: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  session->SetEntityAuthorizationValue("");
  rc = utility->CreateAndLoadRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kSignKey,
      "sign",
      session.get(),
      &signing_key,
      NULL);
  if (rc) {
    LOG(ERROR) << "Error creating key: " << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::ScopedKeyHandle scoped_key(factory, signing_key);
  std::string signature;
  session->SetEntityAuthorizationValue("sign");
  rc = utility->Sign(scoped_key.get(),
                     trunks::TPM_ALG_NULL,
                     trunks::TPM_ALG_NULL,
                     std::string(32, 'a'),
                     session.get(),
                     &signature);
  if (rc) {
    LOG(ERROR) << "Error signing: " << trunks::GetErrorString(rc);
    return rc;
  }
  rc = utility->Verify(scoped_key.get(),
                       trunks::TPM_ALG_NULL,
                       trunks::TPM_ALG_NULL,
                       std::string(32, 'a'),
                       signature);
  if (rc) {
    LOG(ERROR) << "Error verifying: " << trunks::GetErrorString(rc);
    return rc;
  }
  LOG(INFO) << "Test completed successfully.";
  return 0;
}

int DecryptTest() {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_HANDLE decrypt_key;
  trunks::TPM_RC rc;
  scoped_ptr<trunks::AuthorizationSession> session(
      factory.GetAuthorizationSession());
  rc = session->StartUnboundSession(true /* enable encryption */);
  if (rc) {
    LOG(ERROR) << "Error starting authorization session: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  session->SetEntityAuthorizationValue("");
  rc = utility->CreateAndLoadRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      "decrypt",
      session.get(),
      &decrypt_key,
      NULL);
  if (rc) {
    LOG(ERROR) << "Error creating key: " << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::ScopedKeyHandle scoped_key(factory, decrypt_key);
  std::string ciphertext;
  session->SetEntityAuthorizationValue("");
  rc = utility->AsymmetricEncrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  "plaintext",
                                  session.get(),
                                  &ciphertext);
  if (rc) {
    LOG(ERROR) << "Error encrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  std::string plaintext;
  session->SetEntityAuthorizationValue("decrypt");
  rc = utility->AsymmetricDecrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  ciphertext,
                                  session.get(),
                                  &plaintext);
  if (rc) {
    LOG(ERROR) << "Error decrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  CHECK_EQ(plaintext.compare("plaintext"), 0);
  LOG(INFO) << "Test completed successfully.";
  return 0;
}

int ImportTest() {
  trunks::TrunksFactoryImpl factory;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  trunks::TPM_RC rc;
  std::string key_blob;
  scoped_ptr<trunks::AuthorizationSession> session(
      factory.GetAuthorizationSession());
  rc = session->StartUnboundSession(true /* enable encryption */);
  if (rc) {
    LOG(ERROR) << "Error starting authorization session: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  crypto::ScopedRSA rsa(RSA_generate_key(2048, 0x10001, NULL, NULL));
  CHECK(rsa.get());
  std::string modulus(BN_num_bytes(rsa.get()->n), 0);
  BN_bn2bin(rsa.get()->n,
            reinterpret_cast<unsigned char*>(string_as_array(&modulus)));
  std::string prime_factor(BN_num_bytes(rsa.get()->p), 0);
  BN_bn2bin(rsa.get()->p,
            reinterpret_cast<unsigned char*>(string_as_array(&prime_factor)));
  session->SetEntityAuthorizationValue("");
  rc = utility->ImportRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kDecryptAndSignKey,
      modulus,
      0x10001,
      prime_factor,
      "import",
      session.get(),
      &key_blob);
  if (rc) {
    LOG(ERROR) << "Error importings: " << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::TPM_HANDLE key_handle;
  rc = utility->LoadKey(key_blob, session.get(), &key_handle);
  if (rc) {
    LOG(ERROR) << "Error loading: " << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::ScopedKeyHandle scoped_key(factory, key_handle);
  std::string ciphertext;
  session->SetEntityAuthorizationValue("");
  rc = utility->AsymmetricEncrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  "plaintext",
                                  session.get(),
                                  &ciphertext);
  if (rc) {
    LOG(ERROR) << "Error encrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  std::string plaintext;
  session->SetEntityAuthorizationValue("import");
  rc = utility->AsymmetricDecrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  ciphertext,
                                  session.get(),
                                  &plaintext);
  if (rc) {
    LOG(ERROR) << "Error decrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  CHECK_EQ(plaintext.compare("plaintext"), 0);
  LOG(INFO) << "Test completed successfully.";
  return 0;
}

int AuthChangeTest() {
  trunks::TrunksFactoryImpl factory;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  trunks::TPM_RC rc;
  scoped_ptr<trunks::AuthorizationSession> session(
      factory.GetAuthorizationSession());
  rc = session->StartUnboundSession(true /* enable encryption */);
  if (rc) {
    LOG(ERROR) << "Error starting authorization session: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::TPM_HANDLE key_handle;
  session->SetEntityAuthorizationValue("");
  rc = utility->CreateAndLoadRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kDecryptAndSignKey,
      "old_pass",
      session.get(),
      &key_handle,
      NULL);
  if (rc) {
    LOG(ERROR) << "Error creating and loading key: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  std::string key_blob;
  session->SetEntityAuthorizationValue("old_pass");
  rc = utility->ChangeKeyAuthorizationData(key_handle,
                                           "new_pass",
                                           session.get(),
                                           &key_blob);
  if (rc) {
    LOG(ERROR) << "Error changing auth data: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  rc = factory.GetTpm()->FlushContextSync(key_handle, NULL);
  if (rc) {
    LOG(ERROR) << "Error flushing key: " << trunks::GetErrorString(rc);
    return rc;
  }
  session->SetEntityAuthorizationValue("");
  rc = utility->LoadKey(key_blob, session.get(), &key_handle);
  if (rc) {
    LOG(ERROR) << "Error reloading key: " << trunks::GetErrorString(rc);
    return rc;
  }

  trunks::ScopedKeyHandle scoped_key(factory, key_handle);
  std::string ciphertext;
  session->SetEntityAuthorizationValue("");
  rc = utility->AsymmetricEncrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  "plaintext",
                                  session.get(),
                                  &ciphertext);
  if (rc) {
    LOG(ERROR) << "Error encrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  std::string plaintext;
  session->SetEntityAuthorizationValue("new_pass");
  rc = utility->AsymmetricDecrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  ciphertext,
                                  session.get(),
                                  &plaintext);
  if (rc) {
    LOG(ERROR) << "Error decrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  CHECK_EQ(plaintext.compare("plaintext"), 0);
  LOG(INFO) << "Test completed successfully.";
  return 0;
}

int NvramTest(const std::string& owner_password) {
  trunks::TrunksFactoryImpl factory;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  trunks::TPM_RC rc;
  scoped_ptr<trunks::AuthorizationSession> session(
      factory.GetAuthorizationSession());
  rc = session->StartUnboundSession(true /* enable encryption */);
  if (rc) {
    LOG(ERROR) << "Error starting authorization session: "
               << trunks::GetErrorString(rc);
    return rc;
  }
  uint32_t index = 1;
  session->SetEntityAuthorizationValue(owner_password);
  std::string nv_data("nv_data");
  rc = utility->DefineNVSpace(index, nv_data.size(), session.get());
  if (rc) {
    LOG(ERROR) << "Error defining nvram: " << trunks::GetErrorString(rc);
    return rc;
  }
  session->SetEntityAuthorizationValue(owner_password);
  rc = utility->WriteNVSpace(index, 0, nv_data, session.get());
  if (rc) {
    LOG(ERROR) << "Error writing nvram: " << trunks::GetErrorString(rc);
    return rc;
  }
  std::string new_nvdata;
  session->SetEntityAuthorizationValue("");
  rc = utility->ReadNVSpace(index, 0, nv_data.size(),
                            &new_nvdata, session.get());
  if (rc) {
    LOG(ERROR) << "Error reading nvram: " << trunks::GetErrorString(rc);
    return rc;
  }
  CHECK_EQ(0, nv_data.compare(new_nvdata));
  rc = utility->LockNVSpace(index, session.get());
  if (rc) {
    LOG(ERROR) << "Error locking nvram: " << trunks::GetErrorString(rc);
    return rc;
  }
  rc = utility->ReadNVSpace(index, 0, nv_data.size(),
                            &new_nvdata, session.get());
  if (rc) {
    LOG(ERROR) << "Error reading nvram: " << trunks::GetErrorString(rc);
    return rc;
  }
  CHECK_EQ(0, nv_data.compare(new_nvdata));
  session->SetEntityAuthorizationValue(owner_password);
  rc = utility->WriteNVSpace(index, 0, nv_data, session.get());
  if (rc == trunks::TPM_RC_SUCCESS) {
    LOG(ERROR) << "Wrote nvram after locking: " << trunks::GetErrorString(rc);
    return rc;
  }
  session->SetEntityAuthorizationValue(owner_password);
  rc = utility->DestroyNVSpace(index, session.get());
  if (rc) {
    LOG(ERROR) << "Error destroying nvram: " << trunks::GetErrorString(rc);
    return rc;
  }
  LOG(INFO) << "Test completed successfully.";
  return 0;
}

int DumpStatus() {
  trunks::TrunksFactoryImpl factory;
  scoped_ptr<trunks::TpmState> state = factory.GetTpmState();
  trunks::TPM_RC result = state->Initialize();
  if (result != trunks::TPM_RC_SUCCESS) {
    LOG(ERROR) << "Failed to read TPM state: "
               << trunks::GetErrorString(result);
    return result;
  }
  printf("Owner password set: %s\n",
         state->IsOwnerPasswordSet() ? "true" : "false");
  printf("Endorsement password set: %s\n",
         state->IsEndorsementPasswordSet() ? "true" : "false");
  printf("Lockout password set: %s\n",
         state->IsLockoutPasswordSet() ? "true" : "false");
  printf("In lockout: %s\n",
         state->IsInLockout() ? "true" : "false");
  printf("Platform hierarchy enabled: %s\n",
         state->IsOwnerPasswordSet() ? "true" : "false");
  printf("Was shutdown orderly: %s\n",
         state->IsOwnerPasswordSet() ? "true" : "false");
  return 0;
}

}  // namespace


int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("status")) {
    return DumpStatus();
  }
  if (cl->HasSwitch("startup")) {
    return Startup();
  }
  if (cl->HasSwitch("clear")) {
    return Clear();
  }
  if (cl->HasSwitch("init_tpm")) {
    return InitializeTpm();
  }
  if (cl->HasSwitch("help")) {
    puts("Trunks Client: A command line tool to access the TPM.");
    PrintUsage();
    return 0;
  }
  if (cl->HasSwitch("own")) {
    return TakeOwnership(cl->GetSwitchValueASCII("owner_password"));
  }
  if (cl->HasSwitch("regression_test")) {
    int rc;
    LOG(INFO) << "Running RNG test.";
    if ((rc = RNGTest())) { return rc; }
    LOG(INFO) << "Running SignTest.";
    if ((rc = SignTest())) { return rc; }
    LOG(INFO) << "Running DecryptTest.";
    if ((rc = DecryptTest())) { return rc; }
    LOG(INFO) << "Running ImportTest.";
    if ((rc = ImportTest())) { return rc; }
    LOG(INFO) << "Running AuthChangeTest.";
    if ((rc = AuthChangeTest())) { return rc; }
    std::string owner_password;
    if (cl->HasSwitch("owner_password")) {
      owner_password = cl->GetSwitchValueASCII("owner_password");
      LOG(INFO) << "Running NvramTest.";
      if ((rc = NvramTest(owner_password))) { return rc; }
    }
    LOG(INFO) << "All tests were run successfully.";
    return 0;
  }
  puts("Invalid options!");
  PrintUsage();
  return -1;
}
