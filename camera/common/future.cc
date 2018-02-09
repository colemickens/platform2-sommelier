/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros-camera/future.h"

#include <base/time/time.h>

namespace internal {

namespace future_internal {

FutureLock::FutureLock(CancellationRelay* relay)
    : cond_(&lock_), cancelled_(false), signalled_(false), relay_(relay) {
  VLOGF_ENTER();
  if (relay_ && !relay_->AddObserver(this)) {
    cancelled_ = true;
  }
}

FutureLock::~FutureLock() {
  VLOGF_ENTER();
  base::AutoLock l(lock_);
  if (relay_) {
    relay_->RemoveObserver(this);
  }
}

void FutureLock::Signal() {
  VLOGF_ENTER();
  base::AutoLock l(lock_);
  signalled_ = true;
  cond_.Signal();
}

bool FutureLock::Wait(int timeout_ms) {
  VLOGF_ENTER();
  base::AutoLock l(lock_);

  base::TimeTicks end_time =
      base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(timeout_ms);
  while (!signalled_ && !cancelled_) {
    if (timeout_ms > 0) {
      // Wait until the FutureLock is signalled or timeout.
      base::TimeTicks now = base::TimeTicks::Now();
      if (end_time > now) {
        cond_.TimedWait(end_time - now);
      } else {
        LOGF(ERROR) << "FutureLock wait timeout";
        return false;
      }
    } else {
      // Wait indefinitely until the FutureLock is signalled.
      cond_.Wait();
    }
  }

  if (cancelled_) {
    LOGF(ERROR) << "FutureLock was cancelled";
  }
  return !cancelled_;
}

void FutureLock::Cancel() {
  VLOGF_ENTER();
  base::AutoLock l(lock_);
  cancelled_ = true;
  relay_ = nullptr;
  cond_.Signal();
}

}  // namespace future_internal

base::Callback<void()> GetFutureCallback(
    const scoped_refptr<Future<void>>& future) {
  return base::Bind(&Future<void>::Set, future);
}

CancellationRelay::CancellationRelay() : cancelled_(false) {}

CancellationRelay::~CancellationRelay() {
  CancelAllFutures();
}

bool CancellationRelay::AddObserver(future_internal::FutureLock* future_lock) {
  VLOGF_ENTER();
  base::AutoLock l(lock_);
  if (cancelled_) {
    return false;
  }
  observers_.insert(future_lock);
  return true;
}

void CancellationRelay::RemoveObserver(
    future_internal::FutureLock* future_lock) {
  VLOGF_ENTER();
  base::AutoLock l(lock_);
  auto it = observers_.find(future_lock);
  if (it != observers_.end()) {
    observers_.erase(it);
  }
}

void CancellationRelay::CancelAllFutures() {
  VLOGF_ENTER();
  base::AutoLock l(lock_);
  cancelled_ = true;
  for (auto it : observers_) {
    it->Cancel();
  }
  observers_.clear();
}

}  // namespace internal
