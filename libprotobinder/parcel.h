// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBPROTOBINDER_PARCEL_H_
#define LIBPROTOBINDER_PARCEL_H_

#include <stdint.h>
#include <stdlib.h>

#include <linux/android/binder.h>

#include <string>

#include "libprotobinder/binder_export.h"

namespace protobinder {

class IBinder;

class BINDER_EXPORT Parcel {
 public:
  Parcel();
  ~Parcel();

  template <class T>
  bool WriteAligned(T val);

  bool WriteInt32(uint32_t val);
  bool WriteInt64(uint32_t val);
  bool WritePointer(uintptr_t val);
  bool WriteIntFloat(uint32_t val);
  bool Write(void* data, size_t len);
  bool WriteString16(uint16_t* str);
  bool WriteString16FromCString(const char* str);
  bool WriteString(const std::string str);

  bool WriteStrongBinder(IBinder* binder);
  bool WriteObject(const flat_binder_object& object);
  bool WriteRawBinder(void* binder);
  bool WriteRawHandle(uint32_t handle);

  bool WriteFd(int fd);

  bool WriteParcel(Parcel* parcel);

  template <class T>
  bool readAligned(T* pArg);

  template <class T>
  T readAligned();

  uint32_t ReadInt32();
  // bool ReadInt32(uint32_t* val);  // TODO(leecam): Change all reads to this.
  std::string* ReadString16();
  bool ReadString(std::string* new_string);
  IBinder* ReadStrongBinder();
  uintptr_t ReadPointer();

  bool ReadFd(int* fd);

  bool GetFdAtOffset(int* fd, size_t offset);

  bool Read(void* data, size_t len);

  void* Data() const { return reinterpret_cast<void*>(data_); }
  size_t Len() const { return data_len_; }
  size_t Capacity() const { return data_capacity_; }
  bool SetCapacity(size_t capacity);
  bool SetLen(size_t len);
  bool SetPos(size_t pos);

  binder_size_t* ObjectData() const { return objects_; }
  size_t ObjectCount() const { return objects_count_; }

  typedef void (*release_func)(Parcel* parcel,
                               const uint8_t* data,
                               size_t dataSize,
                               const binder_size_t* objects,
                               size_t objectsSize,
                               void* cookie);

  bool InitFromBinderTransaction(void* data,
                                 size_t data_len,
                                 binder_size_t* objects,
                                 size_t objects_count,
                                 release_func func);
  bool IsEmpty() { return data_pos_ >= data_len_; }

 private:
  bool Grow(size_t len);
  void AdvancePostion(size_t len);
  void* AllocatePaddedBuffer(size_t len);
  void* GetPaddedBuffer(size_t len);
  const flat_binder_object* ReadObject();
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
