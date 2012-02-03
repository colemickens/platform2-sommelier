// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POOL_MOCK_H
#define CHAPS_OBJECT_POOL_MOCK_H

#include "chaps/object_pool.h"

#include <base/basictypes.h>
#include <gmock/gmock.h>

namespace chaps {

class Object;

class ObjectPoolMock : public ObjectPool {
 public:
  ObjectPoolMock();
  virtual ~ObjectPoolMock();

  MOCK_METHOD2(GetInternalBlob, bool (int, std::string*));
  MOCK_METHOD2(SetInternalBlob, bool (int, const std::string&));
  MOCK_METHOD1(SetKey, void (const std::string&));
  MOCK_METHOD1(Insert, bool (Object*));
  MOCK_METHOD1(Delete, bool (const Object*));
  MOCK_METHOD2(Find, bool (const Object*, std::vector<const Object*>*));
  MOCK_METHOD1(GetModifiableObject, Object*(const Object*));
  MOCK_METHOD1(Flush, bool(const Object*));
  void SetupFake() {
    ON_CALL(*this, Insert(testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeInsert));
    ON_CALL(*this, Delete(testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeDelete));
    ON_CALL(*this, Find(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeFind));
  }
 private:
  bool FakeInsert(Object* o) {
    v_.push_back(o);
    return true;
  }
  bool FakeDelete(const Object* o) {
    return true;
  }
  bool FakeFind(const Object* o, std::vector<const Object*>* v) {
    for (size_t i = 0; i < v_.size(); ++i)
      v->push_back(v_[i]);
    return true;
  }
  std::vector<const Object*> v_;

  DISALLOW_COPY_AND_ASSIGN(ObjectPoolMock);
};

}  // namespace

#endif  // CHAPS_OBJECT_POOL_MOCK_H
