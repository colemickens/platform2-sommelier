// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_PARCEL_H_
#define LIBPROTOBINDER_PARCEL_H_

#include <stdint.h>
#include <stdlib.h>

#include <linux/android/binder.h>

#include <string>

#include "binder_export.h"  // NOLINT(build/include)

namespace protobinder {

class IBinder;

class BINDER_EXPORT Parcel {
 public:
  Parcel();
  ~Parcel();

  // Basic types.
  bool WriteInt32(int32_t val);
  bool WriteInt64(int64_t val);
  bool WriteUInt32(uint32_t val);
  bool WriteUInt64(uint64_t val);
  bool WriteFloat(float val);
  bool WriteDouble(double val);
  bool WritePointer(uintptr_t val);

  bool ReadInt32(int32_t* val);
  bool ReadInt64(int64_t* val);
  bool ReadUInt32(uint32_t* val);
  bool ReadUInt64(uint64_t* val);
  bool ReadFloat(float* val);
  bool ReadDouble(double* val);
  bool ReadPointer(uintptr_t* val);

  // Buffers and strings.
  bool Write(const void* data, size_t len);
  bool WriteString16(const uint16_t* str, size_t len);
  bool WriteString16(const std::string& str);  // TODO(leecam): Use string16.
  bool WriteString(const std::string& str);

  bool Read(void* data, size_t len);

  // Actual len returned in |len|
  // |len| is number of uint16 not bytes
  bool ReadString16(uint16_t* str, size_t* max_len);
  bool ReadString16(std::string* str);  // TODO(leecam): Use string16.
  bool ReadString(std::string* str);

  // Binder objects.
  bool WriteStrongBinder(const IBinder* binder);
  bool WriteFd(int fd);
  bool WriteRawBinder(const void* binder);
  bool WriteRawHandle(uint32_t handle);

  bool ReadStrongBinder(IBinder** binder);
  bool ReadFd(int* fd);
  bool ReadRawBinder(void** binder);
  bool ReadRawHandle(uint32_t* handle);

  // Allows concating of Parcels.
  bool WriteParcel(Parcel* parcel);

  // Returns a File Descriptor but does not advance
  // the read postion.
  // |offset| is used to index from current read postion.
  bool GetFdAtOffset(int* fd, size_t offset);
  bool GetStrongBinderAtOffset(IBinder** binder, size_t offset);

  // getter/setters for the data buffer.
  void* Data() const { return reinterpret_cast<void*>(data_); }
  size_t Len() const { return data_len_; }
  size_t Capacity() const { return data_capacity_; }
  bool SetCapacity(size_t capacity);
  bool SetLen(size_t len);
  bool SetPos(size_t pos);

  // getter/setters for the object offset buffer.
  binder_size_t* ObjectData() const { return objects_; }
  size_t ObjectCount() const { return objects_count_; }

  bool IsEmpty() { return data_pos_ >= data_len_; }

  typedef void (*release_func)(Parcel* parcel,
                               const uint8_t* data,
                               size_t dataSize,
                               const binder_size_t* objects,
                               size_t objects_size,
                               void* cookie);

  // Can be used configure an empty parcel to use data
  // returned by the binder driver.
  bool InitFromBinderTransaction(void* data,
                                 size_t data_len,
                                 binder_size_t* objects,
                                 size_t objects_size,
                                 release_func func);

 private:
  template <class T>
  bool WriteAligned(T val);
  bool WriteObject(const flat_binder_object& object);

  template <class T>
  bool ReadAligned(T* val);
  const flat_binder_object* ReadObject();

  bool Grow(size_t extra_required);
  void AdvancePostion(size_t len);
  void* AllocatePaddedBuffer(size_t len);
  void* GetPaddedBuffer(size_t len);
  const flat_binder_object* GetObjectAtOffset(size_t offset);

  // Buffer
  uint8_t* data_;         // Data buffer.
  size_t data_len_;       // Length of data within buffer.
  size_t data_capacity_;  // Size of allocated data buffer.
  size_t data_pos_;       // Postion within the buffer.

  // Objects - This is an array of offsets pointing into the buffer.
  // Each offset repersents a special objects such as a binder or
  // file descriptor. The size of an object is always
  // sizeof(struct flat_binder_object).
  // Normal data is not delimited and can interleave with special objects.
  binder_size_t* objects_;
  size_t objects_count_;
  size_t objects_capacity_;

  release_func owners_release_function_;
};

}  // namespace protobinder

#endif  // LIBPROTOBINDER_PARCEL_H_
