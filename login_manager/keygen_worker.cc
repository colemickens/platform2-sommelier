// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/keygen_worker.h"

#include <string>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <crypto/rsa_private_key.h>

#include "login_manager/nss_util.h"
#include "login_manager/owner_key.h"
#include "login_manager/system_utils.h"

namespace login_manager {

namespace keygen {

int generate(const std::string& filename) {
  FilePath key_file(filename);
  scoped_ptr<NssUtil> nss(NssUtil::Create());
  scoped_ptr<OwnerKey> key(new OwnerKey(key_file));
  if (!key->PopulateFromDiskIfPossible())
    LOG(FATAL) << "Corrupted key on disk at " << filename;
  if (key->IsPopulated())
    LOG(FATAL) << "Existing owner key at " << filename;
  if (!nss->OpenUserDB())
    PLOG(FATAL) << "Could not open/create user NSS DB";
  LOG(INFO) << "Generating Owner key.";

  scoped_ptr<crypto::RSAPrivateKey> pair(nss->GenerateKeyPair());
  if (pair.get()) {
    if (!key->PopulateFromKeypair(pair.get()))
      LOG(FATAL) << "Could not use generated keypair.";
    LOG(INFO) << "Writing Owner key to " << key_file.value();
    return (key->Persist() ? 0 : 1);
  }
  LOG(FATAL) << "Could not generate owner key!";
  return 0;
}

}  // namespace keygen

}  // namespace login_manager
