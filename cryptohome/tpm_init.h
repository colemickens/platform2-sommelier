// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TpmInit - public interface class for initializing the TPM

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/utility.h>

#include "attestation.h"
#include "tpm.h"

#ifndef CRYPTOHOME_TPM_INIT_H_
#define CRYPTOHOME_TPM_INIT_H_

namespace cryptohome {

class TpmInitTask;
class Platform;

class TpmInit {
  // Friend class TpmInitTask as it is a glue class to allow ThreadMain to be
  // called on a separate thread without inheriting from
  // PlatformThread::Delegate
  friend class TpmInitTask;
 public:
  class TpmInitCallback {
   public:
    virtual void InitializeTpmComplete(bool status, bool took_ownership) = 0;
  };

  // Default constructor
  explicit TpmInit(Platform* platform);

  virtual ~TpmInit();

  virtual void Init(TpmInitCallback* notify_callback);

  // Gets random data from the TPM
  //
  // Parameters
  //   length - The number of bytes to get
  //   data (OUT) - Receives the random bytes
  virtual bool GetRandomData(int length, chromeos::Blob* data);

  // Starts asynchronous initialization of the TPM
  virtual bool StartInitializeTpm();

  // Returns true if the TPM is initialized and ready for use
  virtual bool IsTpmReady();

  // Returns true if the TPM is enabled
  virtual bool IsTpmEnabled();

  // Returns true if the TPM is owned
  virtual bool IsTpmOwned();

  // Returns true if the TPM is being owned
  virtual bool IsTpmBeingOwned();

  // Returns true if initialization has been called
  virtual bool HasInitializeBeenCalled();

  // Gets the TPM password if the TPM initialization took ownership
  //
  // Parameters
  //   password (OUT) - The owner password used for the TPM
  virtual bool GetTpmPassword(chromeos::Blob* password);

  // Clears the TPM password from memory and disk
  virtual void ClearStoredTpmPassword();

  // Returns true if attestation data has been prepared for enrollment.
  virtual bool IsAttestationPrepared();

  // Returns true if all attestation data can be validated.
  virtual bool VerifyAttestationData();

  // Returns true if the EK certificate can be validated.
  virtual bool VerifyEK();

  virtual void set_tpm(Tpm* value);

  virtual Tpm* get_tpm();

 private:
  virtual void ThreadMain();

  // The background task for initializing the TPM, implemented as a
  // PlatformThread::Delegate
  scoped_ptr<TpmInitTask> tpm_init_task_;
  base::PlatformThreadHandle init_thread_;

  TpmInitCallback* notify_callback_;

  bool initialize_called_;
  bool task_done_;
  bool initialize_took_ownership_;
  int64_t initialization_time_;
  scoped_ptr<Attestation> attestation_;
  Platform* platform_;

  DISALLOW_COPY_AND_ASSIGN(TpmInit);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_TPM_INIT_H_
