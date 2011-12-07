// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
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
  // General state management.
  virtual int GetSlot() const = 0;
  virtual CK_STATE GetState() const = 0;
  virtual bool IsReadOnly() const = 0;
  virtual bool IsOperationActive(OperationType type) const = 0;
  // Object management.
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
  // Cryptographic operations (encrypt, decrypt, digest, sign, verify).
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
  // Key generation.
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
  // Random number generation.
  virtual void SeedRandom(const std::string& seed) = 0;
  virtual void GenerateRandom(int num_bytes, std::string* random_data) = 0;
};

}  // namespace

#endif  // CHAPS_SESSION_H
