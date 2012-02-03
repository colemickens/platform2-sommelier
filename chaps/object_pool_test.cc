// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/object_pool_impl.h"

#include <map>
#include <string>
#include <vector>

#include <base/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/attributes.pb.h"
#include "chaps/chaps_factory_mock.h"
#include "chaps/handle_generator_mock.h"
#include "chaps/object_mock.h"
#include "chaps/object_store_mock.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

static Object* CreateObjectMock() {
  ObjectMock* o = new ObjectMock();
  o->SetupFake();
  EXPECT_CALL(*o, GetObjectClass()).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributes(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, FinalizeNewObject()).WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(*o, Copy(_)).WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(*o, IsTokenObject()).Times(AnyNumber());
  EXPECT_CALL(*o, IsPrivate()).Times(AnyNumber());
  EXPECT_CALL(*o, IsAttributePresent(_)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeString(_)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeInt(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeBool(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributeString(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributeInt(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributeBool(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeMap()).Times(AnyNumber());
  EXPECT_CALL(*o, set_handle(_)).Times(AnyNumber());
  EXPECT_CALL(*o, set_store_id(_)).Times(AnyNumber());
  EXPECT_CALL(*o, handle()).Times(AnyNumber());
  EXPECT_CALL(*o, store_id()).Times(AnyNumber());
  return o;
}

// A test fixture for object pools.
class TestObjectPool : public ::testing::Test {
 public:
  TestObjectPool() {
    // Setup the factory to return functional fake objects.
    EXPECT_CALL(factory_, CreateObject())
        .WillRepeatedly(Invoke(CreateObjectMock));
    EXPECT_CALL(handle_generator_, CreateHandle())
        .WillRepeatedly(Return(1));
    // Create object pools to test with.
    store_ = new ObjectStoreMock();
    pool_.reset(new ObjectPoolImpl(&factory_, &handle_generator_, store_));
    pool2_.reset(new ObjectPoolImpl(&factory_, &handle_generator_, NULL));
  }

  ChapsFactoryMock factory_;
  ObjectStoreMock* store_;
  HandleGeneratorMock handle_generator_;
  scoped_ptr<ObjectPoolImpl> pool_;
  scoped_ptr<ObjectPoolImpl> pool2_;
};

// Test object pool initialization when using an object store.
TEST_F(TestObjectPool, Init) {
  // Create some fake persistent objects for the mock store to return.
  map<int, string> persistent_objects;
  AttributeList l;
  Attribute* a = l.add_attribute();
  a->set_type(CKA_ID);
  a->set_value("value");
  string s;
  l.SerializeToString(&s);
  persistent_objects[1] = s;
  persistent_objects[2] = "not_valid_protobuf";
  EXPECT_CALL(*store_, LoadAllObjectBlobs(_))
      .WillOnce(Return(false))
      .WillRepeatedly(DoAll(SetArgumentPointee<0>(persistent_objects),
                            Return(true)));
  EXPECT_TRUE(pool2_->Init());
  EXPECT_FALSE(pool_->Init());
  EXPECT_TRUE(pool_->Init());
  vector<const Object*> v;
  scoped_ptr<Object> find_all(CreateObjectMock());
  EXPECT_TRUE(pool_->Find(find_all.get(), &v));
  ASSERT_EQ(1, v.size());
  EXPECT_TRUE(v[0]->GetAttributeString(CKA_ID) == string("value"));
}

// Test the methods that should just pass through to the object store.
TEST_F(TestObjectPool, StprePassThrough) {
  string s("test");
  EXPECT_CALL(*store_, GetInternalBlob(1, _))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*store_, SetInternalBlob(1, s))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*store_, SetEncryptionKey(s)).Times(1);
  EXPECT_FALSE(pool2_->GetInternalBlob(1, &s));
  EXPECT_FALSE(pool2_->SetInternalBlob(1, s));
  pool2_->SetKey(s);
  EXPECT_FALSE(pool_->GetInternalBlob(1, &s));
  EXPECT_TRUE(pool_->GetInternalBlob(1, &s));
  EXPECT_FALSE(pool_->SetInternalBlob(1, s));
  EXPECT_TRUE(pool_->SetInternalBlob(1, s));
  pool_->SetKey(s);
}

// Test basic object management operations.
TEST_F(TestObjectPool, InsertFindUpdateDelete) {
  EXPECT_CALL(*store_, InsertObjectBlob(false, CKO_DATA, "", _, _))
      .WillOnce(Return(false))
      .WillRepeatedly(DoAll(SetArgumentPointee<4>(3), Return(true)));
  EXPECT_CALL(*store_, UpdateObjectBlob(3, _))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*store_, DeleteObjectBlob(3))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  vector<const Object*> v;
  scoped_ptr<Object> find_all(CreateObjectMock());
  EXPECT_TRUE(pool2_->Find(find_all.get(), &v));
  EXPECT_EQ(0, v.size());
  EXPECT_TRUE(pool2_->Insert(CreateObjectMock()));
  EXPECT_TRUE(pool2_->Insert(CreateObjectMock()));
  EXPECT_TRUE(pool2_->Find(find_all.get(), &v));
  ASSERT_EQ(2, v.size());
  Object* o = pool2_->GetModifiableObject(v[0]);
  EXPECT_TRUE(pool2_->Flush(o));
  EXPECT_TRUE(pool2_->Delete(v[0]));
  EXPECT_TRUE(pool2_->Delete(v[1]));
  v.clear();
  EXPECT_TRUE(pool2_->Find(find_all.get(), &v));
  EXPECT_EQ(0, v.size());
  // Now with the persistent pool.
  EXPECT_TRUE(pool_->Find(find_all.get(), &v));
  EXPECT_EQ(0, v.size());
  Object* tmp = CreateObjectMock();
  EXPECT_FALSE(pool_->Insert(tmp));
  EXPECT_TRUE(pool_->Insert(tmp));
  EXPECT_TRUE(pool_->Find(find_all.get(), &v));
  ASSERT_EQ(1, v.size());
  o = pool_->GetModifiableObject(v[0]);
  EXPECT_FALSE(pool_->Flush(o));
  EXPECT_TRUE(pool_->Flush(o));
  EXPECT_FALSE(pool_->Delete(v[0]));
  EXPECT_TRUE(pool_->Delete(v[0]));
  v.clear();
  EXPECT_TRUE(pool_->Find(find_all.get(), &v));
  EXPECT_EQ(0, v.size());
}

// Test handling of an invalid object pointer.
TEST_F(TestObjectPool, UnknownObject) {
  scoped_ptr<Object> o(CreateObjectMock());
  EXPECT_FALSE(pool_->Flush(o.get()));
  EXPECT_FALSE(pool_->Delete(o.get()));
  EXPECT_FALSE(pool2_->Flush(o.get()));
  EXPECT_FALSE(pool2_->Delete(o.get()));
}

// Test multiple insertion of the same object pointer.
TEST_F(TestObjectPool, DuplicateObject) {
  Object* o = CreateObjectMock();
  EXPECT_CALL(*store_, InsertObjectBlob(false, CKO_DATA, "", _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<4>(3), Return(true)));
  EXPECT_TRUE(pool_->Insert(o));
  EXPECT_FALSE(pool_->Insert(o));
  Object* o2 = CreateObjectMock();
  EXPECT_TRUE(pool2_->Insert(o2));
  EXPECT_FALSE(pool2_->Insert(o2));
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
