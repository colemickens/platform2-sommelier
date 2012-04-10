// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OBJECT_POOL_MOCK_H
#define CHAPS_OBJECT_POOL_MOCK_H

#include "chaps/object_pool.h"

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "chaps/object.h"

namespace chaps {

class ObjectPoolMock : public ObjectPool {
 public:
  ObjectPoolMock();
  virtual ~ObjectPoolMock();

  MOCK_METHOD2(GetInternalBlob, bool (int, std::string*));
  MOCK_METHOD2(SetInternalBlob, bool (int, const std::string&));
  MOCK_METHOD1(SetEncryptionKey, bool (const std::string&));
  MOCK_METHOD1(Insert, bool (Object*));
  MOCK_METHOD1(Import, bool (Object*));
  MOCK_METHOD1(Delete, bool (const Object*));
  MOCK_METHOD2(Find, bool (const Object*, std::vector<const Object*>*));
  MOCK_METHOD2(FindByHandle, bool(int, const Object**));
  MOCK_METHOD1(GetModifiableObject, Object*(const Object*));
  MOCK_METHOD1(Flush, bool(const Object*));
  void SetupFake() {
    last_handle_ = 0;
    ON_CALL(*this, Insert(testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeInsert));
    ON_CALL(*this, Import(testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeInsert));
    ON_CALL(*this, Delete(testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeDelete));
    ON_CALL(*this, Find(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this, &ObjectPoolMock::FakeFind));
    ON_CALL(*this, FindByHandle(testing::_, testing::_))
        .WillByDefault(testing::Invoke(this,
                                       &ObjectPoolMock::FakeFindByHandle));
  }
 private:
  bool FakeInsert(Object* o) {
    v_.push_back(o);
    o->set_handle(++last_handle_);
    return true;
  }
  bool FakeDelete(const Object* o) {
    for (size_t i = 0; i < v_.size(); ++i) {
      if (o == v_[i]) {
        delete v_[i];
        v_.erase(v_.begin() + i);
        return true;
      }
    }
    return false;
  }
  bool FakeFind(const Object* o, std::vector<const Object*>* v) {
    for (size_t i = 0; i < v_.size(); ++i)
      v->push_back(v_[i]);
    return true;
  }
  bool FakeFindByHandle(int handle, const Object** o) {
    for (size_t i = 0; i < v_.size(); ++i) {
      if (handle == v_[i]->handle()) {
        *o = v_[i];
        return true;
      }
    }
    return false;
  }
  std::vector<const Object*> v_;
  int last_handle_;

  DISALLOW_COPY_AND_ASSIGN(ObjectPoolMock);
};

}  // namespace

#endif  // CHAPS_OBJECT_POOL_MOCK_H
