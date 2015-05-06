// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/parcel.h"

#include <gtest/gtest.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <base/macros.h>

#include "libprotobinder/binder_host.h"

namespace protobinder {

// Note: using ASSERT instead of EXPECT as failure
// results in a Parcel in a undefined state and its
// not safe to continue.

TEST(ParcelTest, BasicTypes) {
  Parcel data;
  size_t total_size = 0;
  ASSERT_TRUE(data.WriteInt32(0xdeadbabe));
  total_size += sizeof(int32_t);
  ASSERT_TRUE(data.WriteUInt32(0xdeadbeef));
  total_size += sizeof(uint32_t);
  ASSERT_TRUE(data.WriteInt64(0xdeadbabedeadbabe));
  total_size += sizeof(int64_t);
  ASSERT_TRUE(data.WriteUInt64(0xdeadbeefdeadbeef));
  total_size += sizeof(uint64_t);
  ASSERT_TRUE(data.WriteFloat(1.234f));
  total_size += sizeof(float);
  ASSERT_TRUE(data.WriteDouble(3.142f));
  total_size += sizeof(double);
  ASSERT_TRUE(data.WritePointer(0xdeadbeef));
  total_size += sizeof(uintptr_t);

  ASSERT_EQ(total_size, data.Len());
  ASSERT_EQ(data.ObjectCount(), 0);

  // Reset pos so data can be read back
  ASSERT_TRUE(data.SetPos(0));
  ASSERT_EQ(total_size, data.Len());

  int32_t int32_val = 0;
  ASSERT_TRUE(data.ReadInt32(&int32_val));
  ASSERT_EQ(int32_val, 0xdeadbabe);
  ASSERT_FALSE(data.IsEmpty());

  uint32_t uint32_val = 0;
  ASSERT_TRUE(data.ReadUInt32(&uint32_val));
  ASSERT_EQ(uint32_val, 0xdeadbeef);
  ASSERT_FALSE(data.IsEmpty());

  int64_t int64_val = 0;
  ASSERT_TRUE(data.ReadInt64(&int64_val));
  ASSERT_EQ(int64_val, 0xdeadbabedeadbabe);
  ASSERT_FALSE(data.IsEmpty());

  uint64_t uint64_val = 0;
  ASSERT_TRUE(data.ReadUInt64(&uint64_val));
  ASSERT_EQ(uint64_val, 0xdeadbeefdeadbeef);
  ASSERT_FALSE(data.IsEmpty());

  float float_val = 0;
  ASSERT_TRUE(data.ReadFloat(&float_val));
  ASSERT_EQ(float_val, 1.234f);
  ASSERT_FALSE(data.IsEmpty());

  double double_val = 0;
  ASSERT_TRUE(data.ReadDouble(&double_val));
  ASSERT_EQ(double_val, 3.142f);
  ASSERT_FALSE(data.IsEmpty());

  uintptr_t pointer_val = 0;
  ASSERT_TRUE(data.ReadPointer(&pointer_val));
  ASSERT_EQ(pointer_val, 0xdeadbeef);
  ASSERT_TRUE(data.IsEmpty());  // Should be empty now

  uint32_t bad_val = 0;
  ASSERT_FALSE(data.ReadUInt32(&bad_val));
}

void CheckBuffer(Parcel* data, const void* buffer, size_t len, bool last) {
  std::vector<char> readback_buffer(len);
  memset(&readback_buffer[0], 0xFF, len);
  ASSERT_TRUE(data->Read(&readback_buffer[0], len));
  ASSERT_EQ(0, memcmp(&readback_buffer[0], buffer, len));
  if (last)
    ASSERT_TRUE(data->IsEmpty());
  else
    ASSERT_FALSE(data->IsEmpty());
}

TEST(ParcelTest, BufferTypes) {
  Parcel data;
  size_t total_size = 0;

  char buffer_aligned_1[1] = "";
  char buffer_aligned_2[2] = "A";
  char buffer_aligned_3[3] = "AB";
  char buffer_aligned_4[4] = "ABC";

  ASSERT_TRUE(data.Write(buffer_aligned_1, sizeof(buffer_aligned_1)));
  total_size += 4;  // Parcel uses 4 byte alignment
  ASSERT_EQ(total_size, data.Len());

  ASSERT_TRUE(data.Write(buffer_aligned_2, sizeof(buffer_aligned_2)));
  total_size += 4;  // Parcel uses 4 byte alignment
  ASSERT_EQ(total_size, data.Len());

  ASSERT_TRUE(data.Write(buffer_aligned_3, sizeof(buffer_aligned_3)));
  total_size += 4;  // Parcel uses 4 byte alignment
  ASSERT_EQ(total_size, data.Len());

  ASSERT_TRUE(data.Write(buffer_aligned_4, sizeof(buffer_aligned_4)));
  total_size += 4;  // Parcel uses 4 byte alignment
  ASSERT_EQ(total_size, data.Len());

  // Large buffer
  const size_t kLargeBufSize = 1024 * 1024;
  std::vector<char> large_buffer(kLargeBufSize);
  memset(&large_buffer[0], 0xAA, kLargeBufSize);
  ASSERT_TRUE(data.Write(&large_buffer[0], kLargeBufSize));
  total_size += kLargeBufSize;
  ASSERT_EQ(total_size, data.Len());

  ASSERT_EQ(data.ObjectCount(), 0);

  // Reset pos so data can be read back
  ASSERT_TRUE(data.SetPos(0));

  CheckBuffer(&data, buffer_aligned_1, sizeof(buffer_aligned_1), false);
  CheckBuffer(&data, buffer_aligned_2, sizeof(buffer_aligned_2), false);
  CheckBuffer(&data, buffer_aligned_3, sizeof(buffer_aligned_3), false);
  CheckBuffer(&data, buffer_aligned_4, sizeof(buffer_aligned_4), false);
  CheckBuffer(&data, &large_buffer[0], kLargeBufSize, true);

  char bad_buffer[1] = "";
  ASSERT_FALSE(data.Read(bad_buffer, sizeof(bad_buffer)));
}

TEST(ParcelTest, StringTypes) {
  Parcel data;

  std::string test_string = "HelloParcel";
  ASSERT_TRUE(data.WriteString(test_string));
  ASSERT_TRUE(data.WriteString(test_string));
  ASSERT_TRUE(data.WriteString(test_string));
  ASSERT_TRUE(data.WriteString(test_string));
  ASSERT_TRUE(data.WriteString16(test_string));
  ASSERT_TRUE(data.WriteString16(test_string));
  ASSERT_TRUE(data.WriteString16(test_string));
  ASSERT_TRUE(data.WriteString16(test_string));

  uint16_t test_string_16[5] = {0x10, 0x20, 0x30, 0x40, 0x50};
  ASSERT_TRUE(data.WriteString16(test_string_16, arraysize(test_string_16)));
  ASSERT_TRUE(data.WriteString16(test_string_16, arraysize(test_string_16)));

  ASSERT_TRUE(data.SetPos(0));

  std::string readback_string;
  ASSERT_TRUE(data.ReadString(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString16(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString16(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString16(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  ASSERT_TRUE(data.ReadString16(&readback_string));
  ASSERT_EQ(0, test_string.compare(readback_string));

  uint16_t readback_string_16[5];
  size_t len;
  len = arraysize(readback_string_16);
  ASSERT_TRUE(data.ReadString16(readback_string_16, &len));
  ASSERT_EQ(0, memcmp(readback_string_16, test_string_16, len));

  len = arraysize(readback_string_16);
  ASSERT_TRUE(data.ReadString16(readback_string_16, &len));
  ASSERT_EQ(0, memcmp(readback_string_16, test_string_16, len));

  ASSERT_TRUE(data.IsEmpty());  // Should be empty now

  ASSERT_FALSE(data.ReadString(&readback_string));
}

TEST(ParcelTest, ObjectTypes) {
  Parcel data;
  size_t total_size = 0;

  // WriteStrongBinder* is covered in integration tests
  // as it requires plumbing with IBinder.

  void* raw_binder = reinterpret_cast<void*>(0xdeadbeef);
  ASSERT_TRUE(data.WriteRawBinder(raw_binder));
  total_size += sizeof(flat_binder_object);

  int fd = 10;
  ASSERT_TRUE(data.WriteFd(fd));
  total_size += sizeof(flat_binder_object);

  uint32_t raw_handle = 0x100;
  ASSERT_TRUE(data.WriteRawHandle(raw_handle));
  total_size += sizeof(flat_binder_object);

  ASSERT_EQ(total_size, data.Len());
  ASSERT_EQ(data.ObjectCount(), 3);

  ASSERT_TRUE(data.SetPos(0));

  void* raw_binder_result = NULL;
  ASSERT_TRUE(data.ReadRawBinder(&raw_binder_result));
  ASSERT_TRUE(raw_binder == raw_binder_result);

  int fd_result = 0;
  ASSERT_TRUE(data.ReadFd(&fd_result));
  ASSERT_EQ(fd, fd_result);

  uint32_t raw_handle_result = 0;
  ASSERT_TRUE(data.ReadRawHandle(&raw_handle_result));
  ASSERT_EQ(raw_handle, raw_handle_result);

  ASSERT_TRUE(data.IsEmpty());  // Should be empty now

  int bad_result = 0;
  ASSERT_FALSE(data.ReadFd(&bad_result));
}

TEST(ParcelTest, ParcelType) {
  Parcel first_parcel, second_parcel;

  // Load up first parcel.
  ASSERT_TRUE(first_parcel.WriteInt32(0x100));
  ASSERT_TRUE(first_parcel.WriteInt32(0x200));
  ASSERT_TRUE(first_parcel.WriteRawHandle(0x100));
  ASSERT_TRUE(first_parcel.WriteRawHandle(0x200));
  ASSERT_EQ(first_parcel.ObjectCount(), 2);

  // Load up second parcel
  ASSERT_TRUE(second_parcel.WriteInt32(0x100));
  ASSERT_TRUE(second_parcel.WriteInt32(0x200));
  ASSERT_TRUE(second_parcel.WriteRawHandle(0x100));
  ASSERT_TRUE(second_parcel.WriteRawHandle(0x200));
  ASSERT_EQ(second_parcel.ObjectCount(), 2);

  ASSERT_TRUE(first_parcel.WriteParcel(&second_parcel));

  ASSERT_EQ(first_parcel.ObjectCount(), 4);

  ASSERT_TRUE(first_parcel.SetPos(0));

  int32_t int32_val = 0;
  uint32_t raw_handle_result = 0;
  // Read back first parcel
  ASSERT_TRUE(first_parcel.ReadInt32(&int32_val));
  ASSERT_EQ(0x100, int32_val);
  ASSERT_TRUE(first_parcel.ReadInt32(&int32_val));
  ASSERT_EQ(0x200, int32_val);
  ASSERT_TRUE(first_parcel.ReadRawHandle(&raw_handle_result));
  ASSERT_EQ(0x100, raw_handle_result);
  ASSERT_TRUE(first_parcel.ReadRawHandle(&raw_handle_result));
  ASSERT_EQ(0x200, raw_handle_result);

  // Read back second parcel from first
  ASSERT_TRUE(first_parcel.ReadInt32(&int32_val));
  ASSERT_EQ(0x100, int32_val);
  ASSERT_TRUE(first_parcel.ReadInt32(&int32_val));
  ASSERT_EQ(0x200, int32_val);
  ASSERT_TRUE(first_parcel.ReadRawHandle(&raw_handle_result));
  ASSERT_EQ(0x100, raw_handle_result);
  ASSERT_TRUE(first_parcel.ReadRawHandle(&raw_handle_result));
  ASSERT_EQ(0x200, raw_handle_result);

  ASSERT_TRUE(first_parcel.IsEmpty());
}

TEST(ParcelTest, FdOffsets) {
  Parcel data;

  // Load parcel up with Fds.
  ASSERT_TRUE(data.WriteFd(1));
  ASSERT_TRUE(data.WriteFd(2));
  ASSERT_TRUE(data.WriteFd(3));
  ASSERT_TRUE(data.WriteFd(4));

  ASSERT_EQ(data.ObjectCount(), 4);

  ASSERT_TRUE(data.SetPos(0));

  int fd = 0;
  ASSERT_TRUE(data.GetFdAtOffset(&fd, 0));
  ASSERT_EQ(1, fd);
  ASSERT_TRUE(data.GetFdAtOffset(&fd, 1));
  ASSERT_EQ(2, fd);
  ASSERT_TRUE(data.GetFdAtOffset(&fd, 2));
  ASSERT_EQ(3, fd);
  ASSERT_TRUE(data.GetFdAtOffset(&fd, 3));
  ASSERT_EQ(4, fd);

  ASSERT_FALSE(data.GetFdAtOffset(&fd, 4));
}

}  // namespace protobinder
