// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/nss_util.h"

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <crypto/nss_util.h>
#include <crypto/rsa_private_key.h>
#include <crypto/signature_creator.h>
#include <crypto/signature_verifier.h>

using crypto::RSAPrivateKey;

namespace {
// This should match the same constant in Chrome tree:
// chrome/browser/chromeos/login/owner_key_utils.cc
const char kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";
}  // namespace

namespace login_manager {
///////////////////////////////////////////////////////////////////////////
// NssUtil

// static
NssUtil::Factory* NssUtil::factory_ = NULL;

NssUtil::NssUtil() {}

NssUtil::~NssUtil() {}

///////////////////////////////////////////////////////////////////////////
// NssUtilImpl

class NssUtilImpl : public NssUtil {
 public:
  NssUtilImpl();
  virtual ~NssUtilImpl();

  bool MightHaveKeys();

  bool OpenUserDB();

  RSAPrivateKey* GetPrivateKey(const std::vector<uint8>& public_key_der);

  RSAPrivateKey* GenerateKeyPair();

  FilePath GetOwnerKeyFilePath();

  bool Verify(const uint8* algorithm, int algorithm_len,
              const uint8* signature, int signature_len,
              const uint8* data, int data_len,
              const uint8* public_key, int public_key_len);

  bool Sign(const uint8* data, int data_len,
            std::vector<uint8>* OUT_signature,
            RSAPrivateKey* key);
 private:
  static const uint16 kKeySizeInBits;
  // Hardcoded path of the user's NSS key database.
  // TODO(cmasone): get rid of this once http://crosbug.com/14007 is fixed.
  static const char kUserDbPath[];

  DISALLOW_COPY_AND_ASSIGN(NssUtilImpl);
};

// Defined here, instead of up above, because we need NssUtilImpl.
// static
NssUtil* NssUtil::Create() {
  if (!factory_) {
    return new NssUtilImpl;
    crypto::EnsureNSSInit();
  } else {
    return factory_->CreateNssUtil();
  }
}

// static
void NssUtil::BlobFromBuffer(const std::string& buf, std::vector<uint8>* out) {
  out->resize(buf.length());
  if (out->size() == 0)
    return;
  memcpy(&(out->at(0)), buf.c_str(), out->size());
}

// We're generating and using 2048-bit RSA keys.
// static
const uint16 NssUtilImpl::kKeySizeInBits = 2048;
// static
const char NssUtilImpl::kUserDbPath[] = "/home/chronos/user/.pki/nssdb/key4.db";

NssUtilImpl::NssUtilImpl() {}

NssUtilImpl::~NssUtilImpl() {}

bool NssUtilImpl::MightHaveKeys() {
  return file_util::PathExists(FilePath(NssUtilImpl::kUserDbPath));
}

bool NssUtilImpl::OpenUserDB() {
  // TODO(cmasone): If we ever try to keep the session_manager alive across
  // user sessions, we'll need to deal with the fact that we have no way to
  // close this persistent DB.
  crypto::OpenPersistentNSSDB();
  return true;
}

RSAPrivateKey* NssUtilImpl::GetPrivateKey(
    const std::vector<uint8>& public_key_der) {
  if (public_key_der.size() == 0) {
    LOG(ERROR) << "Not checking key because size is 0";
    return NULL;
  }
  return RSAPrivateKey::FindFromPublicKeyInfo(public_key_der);
}

RSAPrivateKey* NssUtilImpl::GenerateKeyPair() {
  return RSAPrivateKey::CreateSensitive(kKeySizeInBits);
}

FilePath NssUtilImpl::GetOwnerKeyFilePath() {
  return FilePath(kOwnerKeyFile);
}

// This is pretty much just a blind passthrough, so I won't test it
// in the NssUtil unit tests.  I'll test it from a class that uses this API.
bool NssUtilImpl::Verify(const uint8* algorithm, int algorithm_len,
                         const uint8* signature, int signature_len,
                         const uint8* data, int data_len,
                         const uint8* public_key, int public_key_len) {
  crypto::SignatureVerifier verifier_;

  if (!verifier_.VerifyInit(algorithm, algorithm_len,
                            signature, signature_len,
                            public_key, public_key_len)) {
    LOG(ERROR) << "Could not initialize verifier";
    return false;
  }

  verifier_.VerifyUpdate(data, data_len);
  return (verifier_.VerifyFinal());
}

// This is pretty much just a blind passthrough, so I won't test it
// in the NssUtil unit tests.  I'll test it from a class that uses this API.
bool NssUtilImpl::Sign(const uint8* data, int data_len,
                       std::vector<uint8>* OUT_signature,
                       RSAPrivateKey* key) {
  scoped_ptr<crypto::SignatureCreator> signer(
      crypto::SignatureCreator::Create(key));
  if (!signer->Update(data, data_len))
    return false;
  return signer->Final(OUT_signature);
}

}  // namespace login_manager
