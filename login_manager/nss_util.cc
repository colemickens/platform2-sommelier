// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/nss_util.h"

#include <base/basictypes.h>
#include <base/crypto/rsa_private_key.h>
#include <base/file_path.h>
#include <base/nss_util.h>
#include <base/scoped_ptr.h>
#include <cros/chromeos_login.h>

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

  bool OpenUserDB();

  bool CheckOwnerKey(const std::vector<uint8>& public_key_der);

  FilePath GetOwnerKeyFilePath();

 private:
  DISALLOW_COPY_AND_ASSIGN(NssUtilImpl);
};

// Defined here, instead of up above, because we need NssUtilImpl.
NssUtil* NssUtil::Create() {
  if (!factory_)
    return new NssUtilImpl;
  else
    return factory_->CreateNssUtil();
}

NssUtilImpl::NssUtilImpl() {}

NssUtilImpl::~NssUtilImpl() {}

bool NssUtilImpl::OpenUserDB() {
  base::EnsureNSSInit();
  return true;
}

bool NssUtilImpl::CheckOwnerKey(const std::vector<uint8>& public_key_der) {
  scoped_ptr<base::RSAPrivateKey> pair(
      base::RSAPrivateKey::FindFromPublicKeyInfo(public_key_der));
  return pair.get() != NULL;
}

FilePath NssUtilImpl::GetOwnerKeyFilePath() {
  return FilePath(chromeos::kOwnerKeyFile);
}

}  // namespace login_manager
