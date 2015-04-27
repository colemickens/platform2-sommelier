// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
#define ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_

#include <string>

#include <base/callback_forward.h>

#include "attestation/common/export.h"
#include "attestation/common/interface.pb.h"

namespace attestation {

// The main attestation interface implemented by proxies and services. The
// anticipated flow looks like this:
//   [APP] -> AttestationInterface -> [IPC] -> AttestationInterface
class ATTESTATION_EXPORT AttestationInterface {
 public:
  virtual ~AttestationInterface() = default;

  // Performs initialization tasks that may take a long time. This method must
  // be successfully called before calling any other method. Returns true on
  // success.
  virtual bool Initialize() = 0;

  // Processes a CreateGoogleAttestedKeyRequest and responds with a
  // CreateGoogleAttestedKeyReply.
  using CreateGoogleAttestedKeyCallback =
      base::Callback<void(const CreateGoogleAttestedKeyReply&)>;
  virtual void CreateGoogleAttestedKey(
      const CreateGoogleAttestedKeyRequest& request,
      const CreateGoogleAttestedKeyCallback& callback) = 0;

  // Processes a GetKeyInfoRequest and responds with a GetKeyInfoReply.
  using GetKeyInfoCallback = base::Callback<void(const GetKeyInfoReply&)>;
  virtual void GetKeyInfo(const GetKeyInfoRequest& request,
                          const GetKeyInfoCallback& callback) = 0;

  // Processes a GetEndorsementInfoRequest and responds with a
  // GetEndorsementInfoReply.
  using GetEndorsementInfoCallback =
      base::Callback<void(const GetEndorsementInfoReply&)>;
  virtual void GetEndorsementInfo(
      const GetEndorsementInfoRequest& request,
      const GetEndorsementInfoCallback& callback) = 0;

  // Processes a GetAttestationKeyInfoRequest and responds with a
  // GetAttestationKeyInfoReply.
  using GetAttestationKeyInfoCallback =
      base::Callback<void(const GetAttestationKeyInfoReply&)>;
  virtual void GetAttestationKeyInfo(
      const GetAttestationKeyInfoRequest& request,
      const GetAttestationKeyInfoCallback& callback) = 0;
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
