// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SESSION_MOCK_H
#define CHAPS_SESSION_MOCK_H

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/attributes.h"
#include "chaps/session.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

class SessionMock : public Session {
 public:
  SessionMock() {}
  MOCK_CONST_METHOD0(GetSlot, int ());
  MOCK_CONST_METHOD0(GetState, CK_STATE ());
  MOCK_CONST_METHOD0(IsReadOnly, bool ());
  MOCK_CONST_METHOD1(IsOperationActive, bool (OperationType));
  MOCK_METHOD3(CreateObject, CK_RV (const CK_ATTRIBUTE_PTR,
                                    int,
                                    CK_OBJECT_HANDLE*));
  MOCK_METHOD4(CopyObject, CK_RV (const CK_ATTRIBUTE_PTR,
                                  int,
                                  CK_OBJECT_HANDLE,
                                  CK_OBJECT_HANDLE*));
  MOCK_METHOD1(DestroyObject, CK_RV (CK_OBJECT_HANDLE));
  MOCK_METHOD2(GetObject, bool (CK_OBJECT_HANDLE, Object**));
  MOCK_METHOD2(FindObjectsInit, CK_RV (const CK_ATTRIBUTE_PTR, int));
  MOCK_METHOD2(FindObjects, CK_RV (int, std::vector<CK_OBJECT_HANDLE>*));
  MOCK_METHOD0(FindObjectsFinal, CK_RV ());
  MOCK_METHOD4(OperationInit, CK_RV (OperationType,
                                     CK_MECHANISM_TYPE,
                                     const std::string&,
                                     const Object&));
  MOCK_METHOD3(OperationInit, CK_RV (OperationType,
                                     CK_MECHANISM_TYPE,
                                     const std::string&));
  MOCK_METHOD4(OperationUpdate, CK_RV (OperationType,
                                       const std::string&,
                                       int*,
                                       std::string*));
  MOCK_METHOD2(OperationUpdate, CK_RV (OperationType,
                                       const std::string&));
  MOCK_METHOD3(OperationFinal, CK_RV (OperationType, int*, std::string*));
  MOCK_METHOD2(OperationFinal, CK_RV (OperationType, const std::string&));
  MOCK_METHOD4(OperationSinglePart, CK_RV (OperationType,
                                           const std::string&,
                                           int*,
                                           std::string*));

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionMock);
};

}  // namespace chaps

#endif  // CHAPS_SESSION_MOCK_H
