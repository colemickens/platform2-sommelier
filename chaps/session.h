// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SESSION_H
#define CHAPS_SESSION_H

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
  kSignRecover,
  kVerifyRecover
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
};

}  // namespace

#endif  // CHAPS_SESSION_H
