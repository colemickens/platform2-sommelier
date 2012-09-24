// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class TpmInit

#include "tpm_init.h"

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/time.h>

#include "attestation.h"

using base::kNullThreadHandle;
using base::PlatformThread;

namespace cryptohome {

// TpmInitTask is a private class used to handle asynchronous initialization of
// the TPM.
class TpmInitTask : public PlatformThread::Delegate {
 public:
  explicit TpmInitTask(Platform* platform)
        : tpm_(NULL),
        init_(NULL),
        platform_(platform) {
  }

  virtual ~TpmInitTask() {
  }

  void Init(TpmInit* init) {
    init_ = init;
    if (tpm_)
      tpm_->Init(platform_, false);
  }

  virtual void ThreadMain() {
    if (init_) {
      init_->ThreadMain();
    }
  }

  void set_tpm(Tpm* tpm) {
    tpm_ = tpm;
  }

  Tpm* get_tpm() {
    return tpm_;
  }

 private:
  Tpm* tpm_;
  TpmInit* init_;
  Platform* platform_;
};

TpmInit::TpmInit(Platform* platform)
    : tpm_init_task_(new TpmInitTask(platform)),
      init_thread_(kNullThreadHandle),
      notify_callback_(NULL),
      initialize_called_(false),
      task_done_(false),
      initialize_took_ownership_(false),
      initialization_time_(0),
      platform_(platform) {
}

TpmInit::~TpmInit() {
  if (init_thread_ != kNullThreadHandle) {
    // Must wait for tpm init thread to complete, because when the main thread
    // exits some libtspi data structures are freed.
    PlatformThread::Join(init_thread_);
    init_thread_ = kNullThreadHandle;
  }
}

void TpmInit::set_tpm(Tpm* value) {
  if (tpm_init_task_.get())
    tpm_init_task_->set_tpm(value);
}

Tpm* TpmInit::get_tpm() {
  if (tpm_init_task_.get())
    return tpm_init_task_->get_tpm();
  return NULL;
}

void TpmInit::Init(TpmInitCallback* notify_callback) {
  notify_callback_ = notify_callback;
  tpm_init_task_->Init(this);
  Tpm* tpm = get_tpm();
  // We only want to kick off attestation work if the TPM is owned but the owner
  // password has not been cleared.
  chromeos::SecureBlob password;
  if (tpm && IsTpmReady() && GetTpmPassword(&password)) {
    attestation_.reset(new Attestation(tpm, platform_));
    attestation_->PrepareForEnrollmentAsync();
  }
}

bool TpmInit::GetRandomData(int length, chromeos::Blob* data) {
  return tpm_init_task_->get_tpm()->GetRandomData(length, data);
}

bool TpmInit::StartInitializeTpm() {
  initialize_called_ = true;
  if (!PlatformThread::Create(0, tpm_init_task_.get(), &init_thread_)) {
    LOG(ERROR) << "Unable to create TPM initialization background thread.";
    return false;
  }
  return true;
}

bool TpmInit::IsTpmReady() {
  // The TPM is "ready" if it is enabled, owned, and not being owned.
  return (tpm_init_task_->get_tpm()->IsEnabled() &&
          tpm_init_task_->get_tpm()->IsOwned() &&
          !tpm_init_task_->get_tpm()->IsBeingOwned());
}

bool TpmInit::IsTpmEnabled() {
  return tpm_init_task_->get_tpm()->IsEnabled();
}

bool TpmInit::IsTpmOwned() {
  return tpm_init_task_->get_tpm()->IsOwned();
}

bool TpmInit::IsTpmBeingOwned() {
  return tpm_init_task_->get_tpm()->IsBeingOwned();
}

bool TpmInit::HasInitializeBeenCalled() {
  return initialize_called_;
}

bool TpmInit::GetTpmPassword(chromeos::Blob* password) {
  return tpm_init_task_->get_tpm()->GetOwnerPassword(password);
}

void TpmInit::ClearStoredTpmPassword() {
  tpm_init_task_->get_tpm()->ClearStoredOwnerPassword();
}

bool TpmInit::IsAttestationPrepared() {
  Tpm* tpm = get_tpm();
  if (!tpm || !IsTpmReady())
    return false;
  Attestation attestation(tpm, platform_);
  return attestation.IsPreparedForEnrollment();
}

bool TpmInit::VerifyAttestationData() {
  Tpm* tpm = get_tpm();
  if (!tpm || !IsTpmReady())
    return false;
  Attestation attestation(tpm, platform_);
  return attestation.Verify();
}

bool TpmInit::VerifyEK() {
  Tpm* tpm = get_tpm();
  if (!tpm || !IsTpmReady())
    return false;
  Attestation attestation(tpm, platform_);
  return attestation.VerifyEK();
}

void TpmInit::ThreadMain() {
  base::TimeTicks start = base::TimeTicks::Now();
  bool initialize_result = tpm_init_task_->get_tpm()->InitializeTpm(
      &initialize_took_ownership_);
  base::TimeDelta delta = (base::TimeTicks::Now() - start);
  initialization_time_ = delta.InMilliseconds();
  if (initialize_took_ownership_) {
    LOG(ERROR) << "TPM initialization took " << initialization_time_ << "ms";
  }
  if (notify_callback_) {
    notify_callback_->InitializeTpmComplete(initialize_result,
                                            initialize_took_ownership_);
  }
  Attestation attestation(tpm_init_task_->get_tpm(), platform_);
  attestation.PrepareForEnrollment();
  task_done_ = true;
}

}  // namespace cryptohome
