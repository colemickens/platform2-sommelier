// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
#define ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_

#include <string>

#include <base/callback_forward.h>

#include "attestation/common/interface.pb.h"

namespace attestation {

// The main attestation interface implemented by proxies and services. The
// anticipated flow looks like this:
//   [APP] -> AttestationInterface -> [IPC] -> AttestationInterface
class AttestationInterface {
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

  // Processes a ActivateAttestationKeyRequest and responds with a
  // ActivateAttestationKeyReply.
  using ActivateAttestationKeyCallback =
      base::Callback<void(const ActivateAttestationKeyReply&)>;
  virtual void ActivateAttestationKey(
      const ActivateAttestationKeyRequest& request,
      const ActivateAttestationKeyCallback& callback) = 0;

  // Processes a CreateCertifiableKeyRequest and responds with a
  // CreateCertifiableKeyReply.
  using CreateCertifiableKeyCallback =
      base::Callback<void(const CreateCertifiableKeyReply&)>;
  virtual void CreateCertifiableKey(
      const CreateCertifiableKeyRequest& request,
      const CreateCertifiableKeyCallback& callback) = 0;

  // Processes a DecryptRequest and responds with a DecryptReply.
  using DecryptCallback = base::Callback<void(const DecryptReply&)>;
  virtual void Decrypt(const DecryptRequest& request,
                       const DecryptCallback& callback) = 0;

  // Processes a SignRequest and responds with a SignReply.
  using SignCallback = base::Callback<void(const SignReply&)>;
  virtual void Sign(const SignRequest& request,
                    const SignCallback& callback) = 0;

  // Processes a RegisterKeyWithChapsTokenRequest and responds with a
  // RegisterKeyWithChapsTokenReply.
  using RegisterKeyWithChapsTokenCallback =
      base::Callback<void(const RegisterKeyWithChapsTokenReply&)>;
  virtual void RegisterKeyWithChapsToken(
      const RegisterKeyWithChapsTokenRequest& request,
      const RegisterKeyWithChapsTokenCallback& callback) = 0;
};

}  // namespace attestation

#endif  // ATTESTATION_COMMON_ATTESTATION_INTERFACE_H_
