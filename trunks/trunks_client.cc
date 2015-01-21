// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// trunks_client is a command line tool that supports various TPM operations. It
// does not provide direct access to the trunksd D-Bus interface.

#include <stdio.h>
#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "trunks/error_codes.h"
#include "trunks/password_authorization_delegate.h"
#include "trunks/scoped_key_handle.h"
#include "trunks/tpm_utility.h"
#include "trunks/trunks_factory_impl.h"

namespace {

void PrintUsage() {
  puts("Options:");
  puts("  --help - Prints this message.");
  puts("  --startup - Performs startup and self-tests.");
  puts("  --clear - Clears the TPM. Use before initializing the TPM.");
  puts("  --init_tpm - Initializes a TPM as CrOS firmware does.");
  puts("  --own=<owner_password> - Takes ownership of the TPM with the "
       "given password");
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

int SignTest() {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_HANDLE signing_key;
  trunks::TPM_RC rc;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  rc = utility->CreateAndLoadRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kSignKey,
      "sign",
      &signing_key,
      NULL);
  if (rc) {
    LOG(ERROR) << "Error creating key: " << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::ScopedKeyHandle scoped_key(factory, signing_key);
  std::string signature;
  rc = utility->Sign(scoped_key.get(),
                     trunks::TPM_ALG_NULL,
                     trunks::TPM_ALG_NULL,
                     "sign",
                     std::string(32, 'a'),
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
  return 0;
}

int DecryptTest() {
  trunks::TrunksFactoryImpl factory;
  trunks::TPM_HANDLE decrypt_key;
  trunks::TPM_RC rc;
  scoped_ptr<trunks::TpmUtility> utility = factory.GetTpmUtility();
  rc = utility->CreateAndLoadRSAKey(
      trunks::TpmUtility::AsymmetricKeyUsage::kDecryptKey,
      "decrypt",
      &decrypt_key,
      NULL);
  if (rc) {
    LOG(ERROR) << "Error creating key: " << trunks::GetErrorString(rc);
    return rc;
  }
  trunks::ScopedKeyHandle scoped_key(factory, decrypt_key);
  std::string ciphertext;
  rc = utility->AsymmetricEncrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  "plaintext",
                                  &ciphertext);
  if (rc) {
    LOG(ERROR) << "Error encrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  std::string plaintext;
  rc = utility->AsymmetricDecrypt(scoped_key.get(),
                                  trunks::TPM_ALG_NULL,
                                  trunks::TPM_ALG_NULL,
                                  "decrypt",
                                  ciphertext,
                                  &plaintext);
  if (rc) {
    LOG(ERROR) << "Error decrypting: " << trunks::GetErrorString(rc);
    return rc;
  }
  CHECK_EQ(plaintext.compare("plaintext"), 0);
  return 0;
}

}  // namespace


int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  CommandLine *cl = CommandLine::ForCurrentProcess();
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
    return TakeOwnership(cl->GetSwitchValueASCII("own"));
  }
  if (cl->HasSwitch("sign_test")) {
    return SignTest();
  }
  if (cl->HasSwitch("decrypt_test")) {
    return DecryptTest();
  }
  puts("Invalid options!");
  PrintUsage();
  return -1;
}
