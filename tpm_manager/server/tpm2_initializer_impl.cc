// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_manager/server/tpm2_initializer_impl.h"

#include <string>

#include <base/logging.h>
#include <trunks/tpm_utility.h>
#include <trunks/trunks_factory_impl.h>

#include "tpm_manager/common/local_data.pb.h"

namespace {
const size_t kDefaultPasswordSize = 20;
}  // namespace

namespace tpm_manager {

Tpm2InitializerImpl::Tpm2InitializerImpl(LocalDataStore* local_data_store,
                                         TpmStatus* tpm_status)
    : trunks_factory_(new trunks::TrunksFactoryImpl()),
      openssl_util_(new OpensslCryptoUtil()),
      local_data_store_(local_data_store),
      tpm_status_(tpm_status) {}

Tpm2InitializerImpl::Tpm2InitializerImpl(trunks::TrunksFactory* factory,
                                         OpensslCryptoUtil* openssl_util,
                                         LocalDataStore* local_data_store,
                                         TpmStatus* tpm_status)
    : trunks_factory_(factory),
      openssl_util_(openssl_util),
      local_data_store_(local_data_store),
      tpm_status_(tpm_status) {}

bool Tpm2InitializerImpl::InitializeTpm() {
  if (!SeedTpmRng()) {
    return false;
  }
  if (tpm_status_->IsTpmOwned()) {
    // Tpm is already owned, so we do not need to do anything.
    VLOG(1) << "Tpm already owned.";
    return true;
  }
  // First we read the local data. If the |owned_by_this_install| flag is set,
  // either we did not finish removing owner dependencies or TakeOwnership
  // failed. In either case, we want to retake ownership with the same
  // passwords.
  LocalData local_data;
  if (!local_data_store_->Read(&local_data)) {
    LOG(ERROR) << "Error reading local data.";
    return false;
  }
  std::string owner_password;
  std::string endorsement_password;
  std::string lockout_password;
  // TODO(usanghi): Use owner dependency information rather than the
  // |owned_by_this_install| flag here.
  if (local_data.owned_by_this_install()) {
    owner_password.assign(local_data.owner_password());
    endorsement_password.assign(local_data.endorsement_password());
    lockout_password.assign(local_data.lockout_password());
  } else {
    if (!GetTpmRandomData(kDefaultPasswordSize, &owner_password)) {
      LOG(ERROR) << "Error generating a random owner password.";
      return false;
    }
    if (!GetTpmRandomData(kDefaultPasswordSize, &endorsement_password)) {
      LOG(ERROR) << "Error generating a random endorsement password.";
      return false;
    }
    if (!GetTpmRandomData(kDefaultPasswordSize, &lockout_password)) {
      LOG(ERROR) << "Error generating a random lockout password.";
      return false;
    }
  }
  // We write the passwords to disk, in case there is an error while taking
  // ownership.
  local_data.set_owned_by_this_install(true);
  local_data.set_owner_password(owner_password);
  local_data.set_endorsement_password(endorsement_password);
  local_data.set_lockout_password(lockout_password);
  // TODO(usanghi): Add ownership dependencies here.
  if (!local_data_store_->Write(local_data)) {
    LOG(ERROR) << "Error saving local data.";
    return false;
  }
  trunks::TPM_RC result = trunks_factory_->GetTpmUtility()->TakeOwnership(
      owner_password, endorsement_password, lockout_password);
  if (result != trunks::TPM_RC_SUCCESS) {
    LOG(ERROR) << "Error taking ownership of TPM2.0";
    return false;
  }
  return true;
}

bool Tpm2InitializerImpl::SeedTpmRng() {
  std::string random_bytes;
  if (!openssl_util_->GetRandomBytes(kDefaultPasswordSize, &random_bytes)) {
    return false;
  }
  trunks::TPM_RC result = trunks_factory_->GetTpmUtility()->StirRandom(
      random_bytes, nullptr  /* No Authorization */);
  if (result != trunks::TPM_RC_SUCCESS) {
    return false;
  }
  return true;
}

bool Tpm2InitializerImpl::GetTpmRandomData(size_t num_bytes,
                                           std::string* random_data) {
  trunks::TPM_RC result = trunks_factory_->GetTpmUtility()->GenerateRandom(
      num_bytes, nullptr  /* No Authorization */, random_data);
  if (result != trunks::TPM_RC_SUCCESS) {
    return false;
  }
  return true;
}

}  // namespace tpm_manager
