// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/tpm1_static_utils.h"

#include <memory>

#include <base/logging.h>
#include <base/memory/free_deleter.h>
#include <base/strings/stringprintf.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <trousers/trousers.h>

#include "cryptohome/cryptolib.h"

using brillo::Blob;

using ScopedByteArray = std::unique_ptr<BYTE, base::FreeDeleter>;

namespace cryptohome {

std::string FormatTrousersErrorCode(TSS_RESULT result) {
  return base::StringPrintf("TPM error 0x%x (%s)", result,
                            Trspi_Error_String(result));
}

crypto::ScopedRSA ParseRsaFromTpmPubkeyBlob(const Blob& pubkey) {
  // Parse the serialized TPM_PUBKEY.
  UINT64 offset = 0;
  BYTE* buffer = const_cast<BYTE*>(pubkey.data());
  TPM_PUBKEY parsed;
  TSS_RESULT tss_result = Trspi_UnloadBlob_PUBKEY(&offset, buffer, &parsed);
  if (TPM_ERROR(tss_result)) {
    LOG(ERROR) << "Failed to parse TPM_PUBKEY: "
               << FormatTrousersErrorCode(tss_result);
    return nullptr;
  }
  ScopedByteArray scoped_key(parsed.pubKey.key);
  ScopedByteArray scoped_parms(parsed.algorithmParms.parms);
  TPM_RSA_KEY_PARMS* parms =
      reinterpret_cast<TPM_RSA_KEY_PARMS*>(parsed.algorithmParms.parms);
  crypto::ScopedRSA rsa(RSA_new());
  if (!rsa) {
    LOG(ERROR) << "Failed to create RSA object";
    return nullptr;
  }
  // Get the public exponent.
  if (!parms->exponentSize) {
    rsa->e = BN_new();
    if (!rsa->e) {
      LOG(ERROR) << "Failed to create BN exponent";
      return nullptr;
    }
    BN_set_word(rsa->e, kWellKnownExponent);
  } else {
    rsa->e = BN_bin2bn(parms->exponent, parms->exponentSize, nullptr);
    if (!rsa->e) {
      LOG(ERROR) << "Failed to load BN exponent from TPM_PUBKEY";
      return nullptr;
    }
  }
  // Get the modulus.
  rsa->n = BN_bin2bn(parsed.pubKey.key, parsed.pubKey.keyLength, nullptr);
  if (!rsa->n) {
    LOG(ERROR) << "Failed to load BN modulus from TPM_PUBKEY";
    return nullptr;
  }
  return rsa;
}

}  // namespace cryptohome
