// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_MOCK_H
#define CHAPS_OBJECT_MOCK_H

#include "chaps/object.h"

#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/attributes.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

class ObjectMock : public Object {
 public:
  ObjectMock();
  virtual ~ObjectMock();
  MOCK_CONST_METHOD0(GetStage, ObjectStage ());
  MOCK_CONST_METHOD0(GetObjectClass, CK_OBJECT_CLASS ());
  MOCK_CONST_METHOD0(IsTokenObject, bool ());
  MOCK_CONST_METHOD0(IsModifiable, bool ());
  MOCK_CONST_METHOD0(IsPrivate, bool ());
  MOCK_CONST_METHOD0(GetSize, int ());
  MOCK_METHOD0(FinalizeNewObject, CK_RV ());
  MOCK_METHOD1(Copy, CK_RV (const Object*));
  MOCK_CONST_METHOD2(GetAttributes, CK_RV (CK_ATTRIBUTE_PTR, int));
  MOCK_METHOD2(SetAttributes, CK_RV (const CK_ATTRIBUTE_PTR, int));
  MOCK_CONST_METHOD1(IsAttributePresent, bool (CK_ATTRIBUTE_TYPE));
  MOCK_CONST_METHOD2(GetAttributeBool, bool (CK_ATTRIBUTE_TYPE, bool));
  MOCK_METHOD2(SetAttributeBool, void (CK_ATTRIBUTE_TYPE, bool));
  MOCK_CONST_METHOD2(GetAttributeInt, int (CK_ATTRIBUTE_TYPE, int));
  MOCK_METHOD2(SetAttributeInt, void (CK_ATTRIBUTE_TYPE, int));
  MOCK_CONST_METHOD1(GetAttributeString, std::string (CK_ATTRIBUTE_TYPE));
  MOCK_METHOD2(SetAttributeString, void (CK_ATTRIBUTE_TYPE,
                                         const std::string&));
  MOCK_METHOD1(RemoveAttribute, void (CK_ATTRIBUTE_TYPE));
  MOCK_CONST_METHOD0(GetAttributeMap, const AttributeMap* ());

  void SetupFake() {
    ON_CALL(*this, GetObjectClass())
        .WillByDefault(testing::Invoke(this, &ObjectMock::FakeGetObjectClass));
    ON_CALL(*this, IsTokenObject())
        .WillByDefault(testing::Invoke(this, &ObjectMock::FakeIsTokenObject));
    ON_CALL(*this, SetAttributes(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectMock::FakeSetAttributes));
    ON_CALL(*this, IsAttributePresent(testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeIsAttributePresent));
    ON_CALL(*this, GetAttributeBool(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeGetAttributeBool));
    ON_CALL(*this, GetAttributeString(testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeGetAttributeString));
    ON_CALL(*this, GetAttributeInt(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeGetAttributeInt));
    ON_CALL(*this, SetAttributeBool(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeSetAttributeBool));
    ON_CALL(*this, SetAttributeInt(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeSetAttributeInt));
    ON_CALL(*this, SetAttributeString(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeSetAttributeString));
    ON_CALL(*this, RemoveAttribute(testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectMock::FakeRemoveAttribute));
  }

 private:
  AttributeMap attributes_;
  CK_OBJECT_CLASS FakeGetObjectClass() {
    return FakeGetAttributeInt(CKA_CLASS, 0);
  }
  bool FakeIsTokenObject() {
    return FakeGetAttributeBool(CKA_TOKEN, true);
  }
  bool FakeIsAttributePresent(CK_ATTRIBUTE_TYPE type) {
    return !FakeGetAttributeString(type).empty();
  }
  bool FakeSetAttributes(const CK_ATTRIBUTE_PTR attr, int num_attr) {
    for (int i = 0; i < num_attr; ++i) {
      attributes_[attr[i].type] =
          std::string((char*)attr[i].pValue, attr[i].ulValueLen);
    }
    return CKR_OK;
  }
  bool FakeGetAttributeBool(CK_ATTRIBUTE_TYPE type, bool default_value) {
    std::string s = FakeGetAttributeString(type);
    if (s.empty())
      return default_value;
    return (0 != s[0]);
  }
  int FakeGetAttributeInt(CK_ATTRIBUTE_TYPE type, int default_value) {
    std::string s = FakeGetAttributeString(type);
    if (s.length() < sizeof(int)) {
      return default_value;
    }
    return *(int*)s.data();
  }
  std::string FakeGetAttributeString(CK_ATTRIBUTE_TYPE type) {
    std::string s;
    AttributeMap::iterator it = attributes_.find(type);
    if (it != attributes_.end())
      s = it->second;
    return s;
  }
  void FakeSetAttributeBool(CK_ATTRIBUTE_TYPE type, bool value) {
    attributes_[type] = std::string(1, value ? 1 : 0);
  }
  void FakeSetAttributeInt(CK_ATTRIBUTE_TYPE type, int value) {
    attributes_[type] = std::string((char*)&value, sizeof(int));
  }
  void FakeSetAttributeString(CK_ATTRIBUTE_TYPE type,
                              const std::string& value) {
    attributes_[type] = value;
  }
  void FakeRemoveAttribute(CK_ATTRIBUTE_TYPE type) {
    attributes_.erase(type);
  }

  DISALLOW_COPY_AND_ASSIGN(ObjectMock);
};

}  // namespace chaps

#endif  // CHAPS_OBJECT_MOCK_H
