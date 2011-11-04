// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SESSION_H
#define CHAPS_SESSION_H

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
  virtual int GetSlot() const = 0;
  virtual CK_STATE GetState() const = 0;
  virtual bool IsReadOnly() const = 0;
  virtual bool IsOperationActive(OperationType type) const = 0;
  // Object management.
  virtual CK_RV CreateObject(const Attributes& attributes,
                             CK_OBJECT_HANDLE* new_object_handle) = 0;
  virtual CK_RV CopyObject(const Attributes& attributes,
                           CK_OBJECT_HANDLE object_handle,
                           CK_OBJECT_HANDLE* new_object_handle) = 0;
  virtual CK_RV DestroyObject(CK_OBJECT_HANDLE object_handle) = 0;
};

}  // namespace

#endif  // CHAPS_SESSION_H
