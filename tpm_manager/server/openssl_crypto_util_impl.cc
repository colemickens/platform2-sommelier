// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/openssl_crypto_util_impl.h"

#include <base/logging.h>
#include <base/stl_util.h>
#include <openssl/rand.h>

namespace tpm_manager {

bool OpensslCryptoUtilImpl::GetRandomBytes(size_t num_bytes,
                                           std::string* random_data) {
  random_data->resize(num_bytes);
  unsigned char* random_buffer =
      reinterpret_cast<unsigned char*>(base::data(*random_data));
  if (RAND_bytes(random_buffer, num_bytes) != 1) {
    LOG(ERROR) << "Error getting random bytes using Openssl.";
    random_data->clear();
    return false;
  }
  return true;
}

}  // namespace tpm_manager
