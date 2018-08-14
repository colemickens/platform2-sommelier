// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <chromeos/any.h>
#include <gtest/gtest.h>

using chromeos::internal_details::Buffer;

TEST(Buffer, Empty) {
  Buffer buffer;
  EXPECT_TRUE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kExternal, buffer.storage_);
  EXPECT_EQ(nullptr, buffer.GetDataPtr());
}

TEST(Buffer, Store_Int) {
  Buffer buffer;
  buffer.Assign(2);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kContained, buffer.storage_);
  EXPECT_EQ(typeid(int), buffer.GetDataPtr()->GetType());
}

TEST(Buffer, Store_Double) {
  Buffer buffer;
  buffer.Assign(2.3);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kContained, buffer.storage_);
  EXPECT_EQ(typeid(double), buffer.GetDataPtr()->GetType());
}

TEST(Buffer, Store_Pointers) {
  Buffer buffer;
  // nullptr
  buffer.Assign(nullptr);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kContained, buffer.storage_);
  EXPECT_EQ(typeid(std::nullptr_t), buffer.GetDataPtr()->GetType());

  // char *
  buffer.Assign("abcd");
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kContained, buffer.storage_);
  EXPECT_EQ(typeid(const char*), buffer.GetDataPtr()->GetType());

  // pointer to non-trivial object
  class NonTrivial {
   public:
    virtual ~NonTrivial() {}
  } non_trivial;
  buffer.Assign(&non_trivial);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kContained, buffer.storage_);
  EXPECT_EQ(typeid(NonTrivial*), buffer.GetDataPtr()->GetType());
}

TEST(Buffer, Store_NonTrivialObjects) {
  class NonTrivial {
   public:
    virtual ~NonTrivial() {}
  } non_trivial;
  Buffer buffer;
  buffer.Assign(non_trivial);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kExternal, buffer.storage_);
  EXPECT_EQ(typeid(NonTrivial), buffer.GetDataPtr()->GetType());
}

TEST(Buffer, Store_Objects) {
  Buffer buffer;

  struct Small {
    double d;
  } small = {};
  buffer.Assign(small);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kContained, buffer.storage_);
  EXPECT_EQ(typeid(Small), buffer.GetDataPtr()->GetType());

  struct Large {
    char c[10];
  } large = {};
  buffer.Assign(large);
  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_EQ(Buffer::kExternal, buffer.storage_);
  EXPECT_EQ(typeid(Large), buffer.GetDataPtr()->GetType());
}

TEST(Buffer, Copy) {
  Buffer buffer1;
  Buffer buffer2;

  buffer1.Assign(30);
  buffer1.CopyTo(&buffer2);
  EXPECT_FALSE(buffer1.IsEmpty());
  EXPECT_FALSE(buffer2.IsEmpty());
  EXPECT_EQ(typeid(int), buffer1.GetDataPtr()->GetType());
  EXPECT_EQ(typeid(int), buffer2.GetDataPtr()->GetType());
  EXPECT_EQ(30, buffer1.GetData<int>());
  EXPECT_EQ(30, buffer2.GetData<int>());

  buffer1.Assign(std::string("abc"));
  buffer1.CopyTo(&buffer2);
  EXPECT_FALSE(buffer1.IsEmpty());
  EXPECT_FALSE(buffer2.IsEmpty());
  EXPECT_EQ(typeid(std::string), buffer1.GetDataPtr()->GetType());
  EXPECT_EQ(typeid(std::string), buffer2.GetDataPtr()->GetType());
  EXPECT_EQ("abc", buffer1.GetData<std::string>());
  EXPECT_EQ("abc", buffer2.GetData<std::string>());
}

TEST(Buffer, Move) {
  // Move operations essentially leave the source object in a state that is
  // guaranteed to be safe for reuse or destruction. There is no other explicit
  // guarantees on the exact state of the source after move (e.g. that the
  // source Any will be Empty after the move is complete).
  Buffer buffer1;
  Buffer buffer2;

  buffer1.Assign(30);
  buffer1.MoveTo(&buffer2);
  // Contained types aren't flushed, so the source Any doesn't become empty.
  // The contained value is just moved, but for scalars this just copies
  // the data and any retains the actual type.
  EXPECT_FALSE(buffer1.IsEmpty());
  EXPECT_FALSE(buffer2.IsEmpty());
  EXPECT_EQ(typeid(int), buffer2.GetDataPtr()->GetType());
  EXPECT_EQ(30, buffer2.GetData<int>());

  buffer1.Assign(std::string("abc"));
  buffer1.MoveTo(&buffer2);
  // External types are moved by just moving the pointer value from src to dest.
  // This will make the source object effectively "Empty".
  EXPECT_TRUE(buffer1.IsEmpty());
  EXPECT_FALSE(buffer2.IsEmpty());
  EXPECT_EQ(typeid(std::string), buffer2.GetDataPtr()->GetType());
  EXPECT_EQ("abc", buffer2.GetData<std::string>());
}
