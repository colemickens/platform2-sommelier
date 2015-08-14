// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_SERVER_LOCAL_DATA_STORE_IMPL_H_
#define TPM_MANAGER_SERVER_LOCAL_DATA_STORE_IMPL_H_

#include "tpm_manager/server/local_data_store.h"

#include <string>

#include <base/macros.h>

namespace tpm_manager {

class LocalDataStoreImpl : public LocalDataStore {
 public:
  LocalDataStoreImpl() = default;
  ~LocalDataStoreImpl() override = default;

  // LocalDataStore methods.
  bool Read(LocalData* data) override;
  bool Write(const LocalData& data) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalDataStoreImpl);
};


}  // namespace tpm_manager


#endif  // TPM_MANAGER_SERVER_LOCAL_DATA_STORE_IMPL_H_
