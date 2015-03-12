// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/parcel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "libprotobinder/binder_host.h"
#include "libprotobinder/binder_proxy.h"

#define PAD_SIZE(s) (((s) + 3) & ~3)

namespace protobinder {

Parcel::Parcel()
    : data_(NULL),
      data_len_(0),
      data_capacity_(0),
      data_pos_(0),
      objects_(NULL),
      objects_count_(0),
      objects_capacity_(0),
      owners_release_function_(NULL) {
}

Parcel::~Parcel() {
  if (owners_release_function_) {
    owners_release_function_(this, data_, data_len_, objects_, objects_count_,
                             NULL);
  } else {
    if (data_)
      free(data_);
    if (objects_)
      free(objects_);
  }
}

bool Parcel::InitFromBinderTransaction(void* data,
                                       size_t data_len,
                                       binder_size_t* objects,
                                       size_t objects_count,
                                       Parcel::release_func func) {
  if (data_ != NULL) {
    // Already allocate
    return false;
  }
  data_ = reinterpret_cast<uint8_t*>(data);
  data_len_ = data_len;
  objects_ = objects;
  objects_count_ = objects_count / sizeof(binder_size_t);
  owners_release_function_ = func;
  return true;
}

void Parcel::AdvancePostion(size_t len) {
  data_pos_ += len;
  if (data_pos_ > data_len_)
    data_len_ = data_pos_;
}

bool Parcel::SetCapacity(size_t capacity) {
  // Can't shink yet
  if (capacity < data_capacity_)
    return false;
  size_t new_capacity = PAD_SIZE(capacity);
  uint8_t* new_data = reinterpret_cast<uint8_t*>(realloc(data_, new_capacity));
  if (new_data == NULL)
    return false;
  data_ = new_data;
  data_capacity_ = new_capacity;
  return true;
}

bool Parcel::SetLen(size_t len) {
  if (len > data_capacity_)
    return false;
  data_len_ = len;
  return true;
}

bool Parcel::SetPos(size_t pos) {
  if (pos > data_capacity_)
    return false;
  data_pos_ = pos;
  return true;
}

bool Parcel::Grow(size_t len) {
  size_t new_capacity = PAD_SIZE(data_capacity_ + len) * 2;
  if (new_capacity < data_capacity_)
    return false;
  uint8_t* new_data = reinterpret_cast<uint8_t*>(realloc(data_, new_capacity));
  if (new_data == NULL)
    return false;
  data_ = new_data;
  data_capacity_ = new_capacity;
  return true;
}

void* Parcel::AllocatePaddedBuffer(size_t len) {
  size_t padded_len = PAD_SIZE(len);
  if (padded_len < len || (data_pos_ + padded_len) < data_pos_)
    return NULL;
  if (data_pos_ + padded_len > data_capacity_) {
    if (!Grow(padded_len))
      return NULL;
  }
  uint8_t* data = data_ + data_pos_;
  // If we have padding then zero the last word.
  if (padded_len > len) {
    *reinterpret_cast<uint32_t*>(data + padded_len - sizeof(uint32_t)) = 0;
  }
  AdvancePostion(padded_len);
  return data;
}

bool Parcel::Write(void* data, size_t len) {
  void* buf = AllocatePaddedBuffer(len);
  if (buf == NULL)
    return false;
  memcpy(buf, data, len);
  return true;
}

bool Parcel::WriteString16(uint16_t* str) {
  size_t len = 0;
  while (str[len])
    len++;
  // Only 32bit lengths are supported.
  if (len >= 0x100000000)
    return false;
  if (!WriteInt32(len))
    return false;
  size_t alloc_len = (len + 1) * sizeof(uint16_t);
  if (alloc_len < len)
    return false;
  void* buf = AllocatePaddedBuffer(alloc_len);
  if (buf == NULL)
    return false;
  memcpy(buf, str, alloc_len);
  return true;
}

bool Parcel::WriteString16FromCString(const char* str) {
  size_t len = strlen(str);
  // Only 32bit lengths are supported.
  if (len >= 0x100000000)
    return false;
  if (!WriteInt32(len))
    return false;
  size_t alloc_len = (len + 1) * sizeof(uint16_t);
  if (alloc_len < len)
    return false;
  uint16_t* buf = reinterpret_cast<uint16_t*>(AllocatePaddedBuffer(alloc_len));
  if (buf == NULL)
    return false;
  while (*str)
    *buf++ = *str++;
  *buf++ = 0;
  return true;
}

bool Parcel::WriteString(const std::string str) {
  size_t len = str.size();
  // Only 32bit lengths are supported.
  if (len >= 0x100000000)
    return false;
  if (!WriteInt32(len))
    return false;
  size_t alloc_len = (len + 1) * sizeof(char);
  if (alloc_len < len)
    return false;
  void* buf = AllocatePaddedBuffer(alloc_len);
  if (buf == NULL)
    return false;
  memcpy(buf, str.c_str(), len);
  return true;
}

bool Parcel::WriteRawHandle(uint32_t handle) {
  struct flat_binder_object object;
  object.type = BINDER_TYPE_HANDLE;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.handle = handle;
  object.cookie = 0;
  return WriteObject(object);
}

bool Parcel::WriteRawBinder(void* binder) {
  struct flat_binder_object object;
  object.type = BINDER_TYPE_BINDER;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.binder = (binder_uintptr_t)binder;
  object.cookie = 0;
  return WriteObject(object);
}

bool Parcel::WriteStrongBinder(IBinder* binder) {
  flat_binder_object object;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  if (binder != NULL) {
    IBinder* host = binder->GetBinderHost();
    if (host) {
      object.type = BINDER_TYPE_BINDER;
      object.binder = reinterpret_cast<uintptr_t>(host);
      object.cookie = reinterpret_cast<uintptr_t>(host);
    } else {
      object.type = BINDER_TYPE_HANDLE;
      object.binder = 0;
      object.cookie = 0;
      BinderProxy* proxy = binder->GetBinderProxy();
      if (proxy == NULL) {
        object.handle = 0;
      } else {
        object.handle = proxy->Handle();
      }
    }
  } else {
    object.type = BINDER_TYPE_BINDER;
    object.binder = 0;
    object.cookie = 0;
  }
  return WriteObject(object);
}

bool Parcel::WriteObject(const flat_binder_object& object) {
  if (data_pos_ + sizeof(object) < data_pos_)
    return false;
  if (data_pos_ + sizeof(object) > data_capacity_) {
    // Need to grow data
    if (!Grow(sizeof(object)))
      return false;
  }

  if (objects_count_ >= objects_capacity_) {
    // Need to grow objects
    // TODO(leecam): Is this growth strategy any good?
    size_t new_capacity = objects_capacity_ + 16;
    if (new_capacity < objects_capacity_)
      return false;
    binder_size_t* new_objects =
        reinterpret_cast<binder_size_t*>(realloc(objects_, new_capacity));
    if (new_objects == NULL)
      return false;
    objects_ = new_objects;
    objects_capacity_ = new_capacity;
  }

  // Now have enough room to add the object.
  *reinterpret_cast<flat_binder_object*>(data_ + data_pos_) = object;

  objects_[objects_count_] = data_pos_;
  objects_count_++;

  AdvancePostion(sizeof(object));
  return true;
}

bool Parcel::WriteFd(int fd) {
  struct flat_binder_object object;
  object.type = BINDER_TYPE_FD;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.handle = fd;
  object.cookie = 0;
  return WriteObject(object);
}

bool Parcel::WriteParcel(Parcel* parcel) {
  size_t required_object_count = objects_count_ + parcel->ObjectCount();
  if (required_object_count < objects_count_)
    return false;
  if (required_object_count >= objects_capacity_) {
    size_t new_capacity = required_object_count + 16;
    if (new_capacity < required_object_count)
      return false;
    binder_size_t* new_objects =
        reinterpret_cast<binder_size_t*>(realloc(objects_, new_capacity));
    if (new_objects == NULL)
      return false;
    objects_ = new_objects;
    objects_capacity_ = new_capacity;
  }
  size_t base = data_pos_;

  if (!Write(parcel->Data(), parcel->Len()))
    return false;

  for (size_t i = 0; i < parcel->ObjectCount(); i++) {
    objects_[objects_count_] = base + parcel->ObjectData()[i];
    objects_count_++;
  }
  return true;
}

template <class T>
bool Parcel::WriteAligned(T val) {
  // TODO(leecam): compile time check for alignment
  if (data_pos_ + sizeof(val) < data_pos_)
    return false;
  if (data_pos_ + sizeof(val) > data_capacity_) {
    // Grow the backing buffer
    if (!Grow(sizeof(val)))
      return false;
  }
  *reinterpret_cast<T*>(data_ + data_pos_) = val;
  AdvancePostion(sizeof(val));
  return true;
}

bool Parcel::WriteInt32(uint32_t val) {
  return WriteAligned(val);
}

bool Parcel::WriteInt64(uint32_t val) {
  return WriteAligned(val);
}

bool Parcel::WritePointer(uintptr_t val) {
  return WriteAligned<binder_uintptr_t>(val);
}

bool Parcel::WriteIntFloat(uint32_t val) {
  return WriteAligned(val);
}

void* Parcel::GetPaddedBuffer(size_t len) {
  if ((data_pos_ + PAD_SIZE(len)) >= data_pos_ &&
      (data_pos_ + PAD_SIZE(len)) <= data_len_ && len <= PAD_SIZE(len)) {
    void* data = data_ + data_pos_;
    data_pos_ += PAD_SIZE(len);
    return data;
  }
  return NULL;
}

template <class T>
bool Parcel::readAligned(T* pArg) {
  // TODO(leecam): Compile time check for aligmment
  if (data_pos_ + sizeof(T) < data_pos_)
    return false;
  if ((data_pos_ + sizeof(T)) <= data_len_) {
    const void* data = data_ + data_pos_;
    data_pos_ += sizeof(T);
    *pArg = *reinterpret_cast<const T*>(data);
    return true;
  }
  return false;
}

template <class T>
T Parcel::readAligned() {
  T result;
  if (!readAligned(&result)) {
    result = 0;
  }
  return result;
}

uint32_t Parcel::ReadInt32() {
  return readAligned<uint32_t>();
}

uintptr_t Parcel::ReadPointer() {
  return readAligned<uintptr_t>();
}

bool Parcel::Read(void* data, size_t len) {
  if ((data_pos_ + PAD_SIZE(len)) >= data_pos_ &&
      (data_pos_ + PAD_SIZE(len)) <= data_len_ && len <= PAD_SIZE(len)) {
    memcpy(data, data_ + data_pos_, len);
    data_pos_ += len;
    return true;
  }
  return false;
}

std::string* Parcel::ReadString16() {
  uint32_t len = ReadInt32();
  uint16_t* buf = reinterpret_cast<uint16_t*>(GetPaddedBuffer(len));
  if (buf == NULL)
    return NULL;
  std::string* new_string = new std::string();
  for (uint32_t i = 0; i < len; i++)
    new_string->push_back(buf[i]);
  return new_string;
}

bool Parcel::ReadString(std::string* new_string) {
  uint32_t len = ReadInt32();
  char* buf = reinterpret_cast<char*>(GetPaddedBuffer(len));
  if (buf == NULL)
    return false;
  for (uint32_t i = 0; i < len; i++)
    new_string->push_back(buf[i]);
  return true;
}

const flat_binder_object* Parcel::ReadObject() {
  if (data_pos_ + sizeof(flat_binder_object) < data_pos_)
    return NULL;
  if ((data_pos_ + sizeof(flat_binder_object)) > data_len_)
    return NULL;
  const flat_binder_object* obj =
      reinterpret_cast<const flat_binder_object*>(data_ + data_pos_);
  // TODO(leecam): Validate this object.
  data_pos_ += sizeof(flat_binder_object);
  return obj;
}

const flat_binder_object* Parcel::GetObjectAtOffset(size_t offset) {
  size_t base = data_pos_ + (offset * sizeof(flat_binder_object));
  if (base <  (offset * sizeof(flat_binder_object)))
    return NULL;

  if (base + sizeof(flat_binder_object) < data_pos_)
    return NULL;
  if ((base + sizeof(flat_binder_object)) > data_len_)
    return NULL;
  const flat_binder_object* obj =
      reinterpret_cast<const flat_binder_object*>(data_ + base);
  // TODO(leecam): Validate this object
  return obj;
}

IBinder* Parcel::ReadStrongBinder() {
  const flat_binder_object* flat_object = ReadObject();
  if (flat_object == NULL)
    return NULL;

  switch (flat_object->type) {
    case BINDER_TYPE_BINDER:
      return NULL;
    case BINDER_TYPE_HANDLE:
      return new BinderProxy(flat_object->handle);
  }
  return NULL;
}

bool Parcel::ReadFd(int* fd) {
  const flat_binder_object* flat_object = ReadObject();
  if (flat_object == NULL)
    return false;
  if (flat_object->type != BINDER_TYPE_FD)
    return false;
  *fd = flat_object->handle;
  return true;
}

bool Parcel::GetFdAtOffset(int* fd, size_t offset) {
  const flat_binder_object* flat_object = GetObjectAtOffset(offset);
  if (flat_object == NULL)
    return false;
  if (flat_object->type != BINDER_TYPE_FD)
    return false;
  *fd = flat_object->handle;
  return true;
}

}  // namespace protobinder

