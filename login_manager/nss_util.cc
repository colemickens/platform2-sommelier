// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/nss_util.h"

#include <string>
#include <utility>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>
#include <crypto/nss_util.h>
#include <crypto/nss_util_internal.h>
#include <crypto/rsa_private_key.h>
#include <crypto/scoped_nss_types.h>
#include <crypto/signature_creator.h>
#include <crypto/signature_verifier.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <prerror.h>
#include <secmod.h>
#include <secmodt.h>

using crypto::RSAPrivateKey;
using crypto::ScopedPK11Slot;
using crypto::ScopedSECItem;
using crypto::ScopedSECKEYPublicKey;
using crypto::ScopedSECKEYPrivateKey;

namespace {
// This should match the same constant in Chrome tree:
// chrome/browser/chromeos/settings/owner_key_util.cc
const char kOwnerKeyFile[] = "/var/lib/whitelist/owner.key";
}  // namespace

namespace login_manager {
///////////////////////////////////////////////////////////////////////////
// NssUtil

NssUtil::NssUtil() {}

NssUtil::~NssUtil() {}

///////////////////////////////////////////////////////////////////////////
// NssUtilImpl

class NssUtilImpl : public NssUtil {
 public:
  NssUtilImpl();
  virtual ~NssUtilImpl();

  virtual ScopedPK11Slot OpenUserDB(
      const base::FilePath& user_homedir) OVERRIDE;

  virtual RSAPrivateKey* GetPrivateKeyForUser(
      const std::vector<uint8>& public_key_der,
      PK11SlotInfo* user_slot) OVERRIDE;

  virtual RSAPrivateKey* GenerateKeyPairForUser(
      PK11SlotInfo* user_slot) OVERRIDE;

  virtual base::FilePath GetOwnerKeyFilePath() OVERRIDE;

  virtual base::FilePath GetNssdbSubpath() OVERRIDE;

  virtual bool CheckPublicKeyBlob(const std::vector<uint8>& blob) OVERRIDE;

  virtual bool Verify(const uint8* algorithm, int algorithm_len,
                      const uint8* signature, int signature_len,
                      const uint8* data, int data_len,
                      const uint8* public_key, int public_key_len) OVERRIDE;

  virtual bool Sign(const uint8* data, int data_len,
                    std::vector<uint8>* OUT_signature,
                    RSAPrivateKey* key) OVERRIDE;
 private:
  static const uint16 kKeySizeInBits;
  static const char kNssdbSubpath[];

  DISALLOW_COPY_AND_ASSIGN(NssUtilImpl);
};

// Defined here, instead of up above, because we need NssUtilImpl.
// static
NssUtil* NssUtil::Create() {
  return new NssUtilImpl;
}

// static
void NssUtil::BlobFromBuffer(const std::string& buf, std::vector<uint8>* out) {
  out->resize(buf.length());
  if (out->size() == 0)
    return;
  memcpy(&((*out)[0]), buf.c_str(), out->size());
}

// We're generating and using 2048-bit RSA keys.
// static
const uint16 NssUtilImpl::kKeySizeInBits = 2048;
// static
const char NssUtilImpl::kNssdbSubpath[] = ".pki/nssdb";

NssUtilImpl::NssUtilImpl() {
  crypto::EnsureNSSInit();
}

NssUtilImpl::~NssUtilImpl() {}

ScopedPK11Slot NssUtilImpl::OpenUserDB(
    const base::FilePath& user_homedir) {
  // TODO(cmasone): If we ever try to keep the session_manager alive across
  // user sessions, we'll need to close these persistent DBs.
  base::FilePath db_path(user_homedir.AppendASCII(kNssdbSubpath));
  const std::string modspec =
      base::StringPrintf("configDir='sql:%s' tokenDescription='%s'",
                         db_path.value().c_str(),
                         user_homedir.value().c_str());
  ScopedPK11Slot db_slot(SECMOD_OpenUserDB(modspec.c_str()));
  if (!db_slot.get()) {
    LOG(ERROR) << "Error opening persistent database (" << modspec
               << "): " << PR_GetError();
    return ScopedPK11Slot();
  }
  if (PK11_NeedUserInit(db_slot.get()))
    PK11_InitPin(db_slot.get(), NULL, NULL);

  // If we opened successfully, we will have a non-default private key slot.
  if (PK11_IsInternalKeySlot(db_slot.get()))
    return ScopedPK11Slot();

  return ScopedPK11Slot(db_slot.get());
}

RSAPrivateKey* NssUtilImpl::GetPrivateKeyForUser(
    const std::vector<uint8>& public_key_der,
    PK11SlotInfo* user_slot) {
  if (public_key_der.size() == 0) {
    LOG(ERROR) << "Not checking key because size is 0";
    return NULL;
  }

  // First, decode and save the public key.
  SECItem key_der;
  key_der.type = siBuffer;
  key_der.data = const_cast<unsigned char*>(&public_key_der[0]);
  key_der.len = public_key_der.size();

  CERTSubjectPublicKeyInfo* spki =
      SECKEY_DecodeDERSubjectPublicKeyInfo(&key_der);
  if (!spki) {
    NOTREACHED();
    return NULL;
  }

  ScopedSECKEYPublicKey public_key(SECKEY_ExtractPublicKey(spki));
  SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (!public_key.get()) {
    NOTREACHED();
    return NULL;
  }

  // Make sure the key is an RSA key.  If not, that's an error
  if (SECKEY_GetPublicKeyType(public_key.get()) != rsaKey) {
    NOTREACHED();
    return NULL;
  }

  ScopedSECItem ck_id(PK11_MakeIDFromPubKey(&(public_key->u.rsa.modulus)));
  if (!ck_id.get()) {
    NOTREACHED();
    return NULL;
  }

  // Search in just the user slot for the key with the given ID.
  ScopedSECKEYPrivateKey key(PK11_FindKeyByKeyID(user_slot,
                                                 ck_id.get(),
                                                 NULL));
  if (key.get())
    return RSAPrivateKey::CreateFromKey(key.get());

  // We didn't find the key.
  return NULL;
}

RSAPrivateKey* NssUtilImpl::GenerateKeyPairForUser(PK11SlotInfo* user_slot) {
  PK11RSAGenParams param;
  param.keySizeInBits = kKeySizeInBits;
  param.pe = 65537L;
  SECKEYPublicKey* public_key_ptr = NULL;
  ScopedSECKEYPrivateKey key(PK11_GenerateKeyPair(user_slot,
                                                  CKM_RSA_PKCS_KEY_PAIR_GEN,
                                                  &param,
                                                  &public_key_ptr,
                                                  PR_TRUE /* permanent */,
                                                  PR_TRUE /* sensitive */,
                                                  NULL));
  ScopedSECKEYPublicKey public_key(public_key_ptr);
  if (!key.get())
    return NULL;

  return RSAPrivateKey::CreateFromKey(key.get());
}

base::FilePath NssUtilImpl::GetOwnerKeyFilePath() {
  return base::FilePath(kOwnerKeyFile);
}

base::FilePath NssUtilImpl::GetNssdbSubpath() {
  return FilePath(kNssdbSubpath);
}

bool NssUtilImpl::CheckPublicKeyBlob(const std::vector<uint8>& blob) {
  CERTSubjectPublicKeyInfo* spki = NULL;
  SECItem spki_der;
  spki_der.type = siBuffer;
  spki_der.data = const_cast<uint8*>(&blob[0]);
  spki_der.len = blob.size();
  spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&spki_der);
  if (!spki)
    return false;

  ScopedSECKEYPublicKey public_key(SECKEY_ExtractPublicKey(spki));
  SECKEY_DestroySubjectPublicKeyInfo(spki);  // Done with spki.
  if (!public_key.get())
   return false;
  return true;
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
