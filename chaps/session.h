// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SESSION_H
#define CHAPS_SESSION_H

#include <string>
#include <vector>

#include "chaps/object.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

enum OperationType {
  kEncrypt,
  kDecrypt,
  kDigest,
  kSign,
  kVerify,
};

// Session is the interface for a PKCS #11 session.  This component is
// responsible for maintaining session state including the state of any multi-
// part operations and any session objects.  It is also responsible for
// executing all session-specific operations.
class Session {
 public:
  virtual ~Session() {}
  // General state management (see PKCS #11 v2.20: 11.6 C_GetSessionInfo).
  virtual int GetSlot() const = 0;
  virtual CK_STATE GetState() const = 0;
  virtual bool IsReadOnly() const = 0;
  virtual bool IsOperationActive(OperationType type) const = 0;
  // Object management (see PKCS #11 v2.20: 11.7).
  virtual CK_RV CreateObject(const CK_ATTRIBUTE_PTR attributes,
                             int num_attributes,
                             CK_OBJECT_HANDLE* new_object_handle) = 0;
  virtual CK_RV CopyObject(const CK_ATTRIBUTE_PTR attributes,
                           int num_attributes,
                           CK_OBJECT_HANDLE object_handle,
                           CK_OBJECT_HANDLE* new_object_handle) = 0;
  virtual CK_RV DestroyObject(CK_OBJECT_HANDLE object_handle) = 0;
  virtual bool GetObject(CK_OBJECT_HANDLE object_handle, Object** object) = 0;
  virtual CK_RV FindObjectsInit(const CK_ATTRIBUTE_PTR attributes,
                                int num_attributes) = 0;
  virtual CK_RV FindObjects(int max_object_count,
                            std::vector<CK_OBJECT_HANDLE>* object_handles) = 0;
  virtual CK_RV FindObjectsFinal() = 0;
  // Cryptographic operations (encrypt, decrypt, digest, sign, verify). See
  // PKCS #11 v2.20: 11.8 through 11.12 for details on these operations. See
  // section 11.2 for a description of PKCS #11 operation output semantics.
  //   All methods providing output use the following parameters:
  //     required_out_length - Provides the maximum output receivable on input
  //                           and is populated with the required output length.
  //     data_out - Is populated with output data if the required output length
  //                is not greater than the maximum receivable length.
  //                Otherwise, the method must be called again with an
  //                appropriate maximum in order to receive the output. All
  //                input will be ignored until the output has been received by
  //                the caller.
  virtual CK_RV OperationInit(OperationType operation,
                              CK_MECHANISM_TYPE mechanism,
                              const std::string& mechanism_parameter,
                              const Object& key) = 0;
  virtual CK_RV OperationInit(OperationType operation,
                              CK_MECHANISM_TYPE mechanism,
                              const std::string& mechanism_parameter) = 0;
  virtual CK_RV OperationUpdate(OperationType operation,
                                const std::string& data_in,
                                int* required_out_length,
                                std::string* data_out) = 0;
  virtual CK_RV OperationUpdate(OperationType operation,
                                const std::string& data_in) = 0;
  virtual CK_RV OperationFinal(OperationType operation,
                               int* required_out_length,
                               std::string* data_out) = 0;
  virtual CK_RV OperationFinal(OperationType operation,
                               const std::string& data_in) = 0;
  virtual CK_RV OperationSinglePart(OperationType operation,
                                    const std::string& data_in,
                                    int* required_out_length,
                                    std::string* data_out) = 0;
  // Key generation (see PKCS #11 v2.20: 11.14).
  virtual CK_RV GenerateKey(CK_MECHANISM_TYPE mechanism,
                            const std::string& mechanism_parameter,
                            const CK_ATTRIBUTE_PTR attributes,
                            int num_attributes,
                            CK_OBJECT_HANDLE* new_key_handle) = 0;
  virtual CK_RV GenerateKeyPair(CK_MECHANISM_TYPE mechanism,
                                const std::string& mechanism_parameter,
                                const CK_ATTRIBUTE_PTR public_attributes,
                                int num_public_attributes,
                                const CK_ATTRIBUTE_PTR private_attributes,
                                int num_private_attributes,
                                CK_OBJECT_HANDLE* new_public_key_handle,
                                CK_OBJECT_HANDLE* new_private_key_handle) = 0;
  // Random number generation (see PKCS #11 v2.20: 11.15).
  virtual void SeedRandom(const std::string& seed) = 0;
  virtual void GenerateRandom(int num_bytes, std::string* random_data) = 0;
};

}  // namespace

#endif  // CHAPS_SESSION_H
