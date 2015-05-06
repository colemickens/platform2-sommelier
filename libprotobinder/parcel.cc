// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libprotobinder/parcel.h"

#include <string.h>

#include "libprotobinder/binder.pb.h"
#include "libprotobinder/binder_host.h"
#include "libprotobinder/binder_proxy.h"

#define PAD_SIZE(s) (((s) + 3) & ~3)

namespace protobinder {

Parcel::Parcel()
    : data_(nullptr),
      data_len_(0),
      data_capacity_(0),
      data_pos_(0),
      objects_(nullptr),
      objects_count_(0),
      objects_capacity_(0) {
}

Parcel::~Parcel() {
  if (!release_callback_.is_null()) {
    release_callback_.Run(this);
  } else {
    if (data_)
      free(data_);
    if (objects_)
      free(objects_);
  }
}

// ---- Public API ----

bool Parcel::WriteInt32(int32_t val) {
  return WriteAligned(val);
}

bool Parcel::WriteInt64(int64_t val) {
  return WriteAligned(val);
}

bool Parcel::WriteUInt32(uint32_t val) {
  return WriteAligned(val);
}

bool Parcel::WriteUInt64(uint64_t val) {
  return WriteAligned(val);
}

bool Parcel::WriteFloat(float val) {
  return WriteAligned(val);
}

bool Parcel::WriteDouble(double val) {
  return WriteAligned(val);
}

bool Parcel::WritePointer(uintptr_t val) {
  return WriteAligned<binder_uintptr_t>(val);
}

bool Parcel::ReadInt32(int32_t* val) {
  return ReadAligned(val);
}

bool Parcel::ReadInt64(int64_t* val) {
  return ReadAligned(val);
}

bool Parcel::ReadUInt32(uint32_t* val) {
  return ReadAligned(val);
}

bool Parcel::ReadUInt64(uint64_t* val) {
  return ReadAligned(val);
}

bool Parcel::ReadFloat(float* val) {
  return ReadAligned(val);
}

bool Parcel::ReadDouble(double* val) {
  return ReadAligned(val);
}

bool Parcel::ReadPointer(uintptr_t* val) {
  binder_uintptr_t pointer_val = 0;
  if (ReadAligned(&pointer_val)) {
    *val = pointer_val;
    return true;
  }
  return false;
}

bool Parcel::Write(const void* data, size_t len) {
  void* buf = AllocatePaddedBuffer(len);
  if (buf == nullptr)
    return false;
  memcpy(buf, data, len);
  return true;
}

bool Parcel::WriteString16(const uint16_t* str, size_t len) {
  // Only 32bit lengths are supported.
  if (static_cast<uint64_t>(len) >= 0x100000000)
    return false;
  size_t alloc_len = len * sizeof(uint16_t);
  if (alloc_len < len)
    return false;
  if (!WriteUInt32(len))
    return false;
  return Write(str, alloc_len);
}

bool Parcel::WriteString16(const std::string& str) {
  size_t len = str.size();
  // Only 32bit lengths are supported.
  if (static_cast<uint64_t>(len) >= 0x100000000)
    return false;
  // Android String16 Parcel code expects a NULL byte
  size_t alloc_len = (len + 1) * sizeof(uint16_t);
  if (alloc_len <= len)
    return false;
  if (!WriteUInt32(len))
    return false;
  uint16_t* buf = reinterpret_cast<uint16_t*>(AllocatePaddedBuffer(alloc_len));
  if (buf == nullptr)
    return false;
  for (size_t i = 0; i < len; i++)
    buf[i] = str[i];
  buf[len] = '\0';
  return true;
}

bool Parcel::WriteString(const std::string& str) {
  size_t len = str.size();
  // Only 32bit lengths are supported.
  if (static_cast<uint64_t>(len) >= 0x100000000)
    return false;
  if (!WriteUInt32(len))
    return false;
  // No need to store NULL
  return Write(str.c_str(), len);
}

bool Parcel::Read(void* data, size_t len) {
  if ((data_pos_ + PAD_SIZE(len)) >= data_pos_ &&
      (data_pos_ + PAD_SIZE(len)) <= data_len_ && len <= PAD_SIZE(len)) {
    memcpy(data, data_ + data_pos_, len);
    data_pos_ += PAD_SIZE(len);
    return true;
  }
  return false;
}

// TODO(leecam): Replace most of these nullptr checks with a DCHECK().
// http://brbug.com/599
bool Parcel::ReadString16(uint16_t* new_string, size_t* max_len) {
  if (new_string == nullptr || max_len == nullptr)
    return false;
  uint32_t len = 0;
  if (!ReadUInt32(&len))
    return false;
  if (len > *max_len)
    return false;
  size_t alloc_len = len * sizeof(uint16_t);
  if (alloc_len < len)
    return false;
  uint16_t* buf = reinterpret_cast<uint16_t*>(GetPaddedBuffer(alloc_len));
  if (buf == nullptr)
    return false;
  memcpy(new_string, buf, alloc_len);
  *max_len = len;
  return true;
}

bool Parcel::ReadString16(std::string* new_string) {
  if (new_string == nullptr)
    return false;
  uint32_t len = 0;
  if (!ReadUInt32(&len))
    return false;
  size_t alloc_len = (len + 1) * sizeof(uint16_t);
  if (alloc_len <= len)
    return false;
  uint16_t* buf = reinterpret_cast<uint16_t*>(GetPaddedBuffer(alloc_len));
  if (buf == nullptr)
    return false;
  new_string->clear();
  for (size_t i = 0; i < len; i++)
    new_string->push_back(buf[i]);
  return true;
}

bool Parcel::ReadString(std::string* new_string) {
  if (new_string == nullptr)
    return false;
  uint32_t len = 0;
  if (!ReadUInt32(&len))
    return false;
  char* buf = reinterpret_cast<char*>(GetPaddedBuffer(len));
  if (buf == nullptr)
    return false;
  new_string->clear();
  new_string->insert(0, buf, len);
  return true;
}

bool Parcel::WriteStrongBinderFromProtocolBuffer(const StrongBinder& binder) {
  flat_binder_object object;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.type = BINDER_TYPE_BINDER;
  object.binder = 0;
  object.handle = 0;
  object.cookie = 0;

  if (binder.has_host_cookie()) {
    object.type = BINDER_TYPE_BINDER;
    object.binder = static_cast<binder_uintptr_t>(binder.host_cookie());
    object.cookie = object.binder;
  } else if (binder.has_proxy_handle()) {
    object.type = BINDER_TYPE_HANDLE;
    object.handle = binder.proxy_handle();
  }

  return WriteObject(object);
}

bool Parcel::WriteStrongBinderFromIBinder(const IBinder& binder) {
  flat_binder_object object;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.type = BINDER_TYPE_BINDER;
  object.binder = 0;
  object.handle = 0;
  object.cookie = 0;

  if (binder.GetBinderHost()) {
    object.type = BINDER_TYPE_BINDER;
    object.binder =
        static_cast<binder_uintptr_t>(binder.GetBinderHost()->cookie());
    object.cookie = object.binder;
  } else if (binder.GetBinderProxy()) {
    object.type = BINDER_TYPE_HANDLE;
    object.handle = binder.GetBinderProxy()->handle();
  } else {
    LOG(FATAL) << "IBinder is neither host nor proxy";
  }

  return WriteObject(object);
}

bool Parcel::WriteFd(int fd) {
  struct flat_binder_object object;
  object.type = BINDER_TYPE_FD;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.handle = fd;
  object.cookie = 0;
  return WriteObject(object);
}

bool Parcel::WriteRawHandle(uint32_t handle) {
  struct flat_binder_object object;
  object.type = BINDER_TYPE_HANDLE;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.handle = handle;
  object.cookie = 0;
  return WriteObject(object);
}

bool Parcel::WriteRawBinder(const void* binder) {
  struct flat_binder_object object;
  object.type = BINDER_TYPE_BINDER;
  object.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
  object.binder = reinterpret_cast<binder_uintptr_t>(binder);
  object.cookie = 0;
  return WriteObject(object);
}

bool Parcel::ReadStrongBinderToIBinder(IBinder** binder) {
  if (!binder)
    return false;
  const flat_binder_object* flat_object = ReadObject();
  if (!flat_object)
    return false;

  switch (flat_object->type) {
    case BINDER_TYPE_BINDER:
      // TODO(leecam): Support this and return flat->cookie
      return false;
    case BINDER_TYPE_HANDLE:
      *binder = new BinderProxy(flat_object->handle);
      return true;
  }
  return false;
}

bool Parcel::ReadFd(int* fd) {
  if (fd == nullptr)
    return false;
  const flat_binder_object* flat_object = ReadObject();
  if (flat_object == nullptr)
    return false;
  if (flat_object->type != BINDER_TYPE_FD)
    return false;
  *fd = flat_object->handle;
  return true;
}

bool Parcel::ReadRawBinder(void** binder) {
  if (binder == nullptr)
    return false;
  const flat_binder_object* flat_object = ReadObject();
  if (flat_object == nullptr)
    return false;
  if (flat_object->type != BINDER_TYPE_BINDER)
    return false;
  *binder = reinterpret_cast<void*>(flat_object->binder);
  return true;
}

bool Parcel::ReadRawHandle(uint32_t* handle) {
  if (handle == nullptr)
    return false;
  const flat_binder_object* flat_object = ReadObject();
  if (flat_object == nullptr)
    return false;
  if (flat_object->type != BINDER_TYPE_HANDLE)
    return false;
  *handle = flat_object->handle;
  return true;
}

bool Parcel::WriteParcel(Parcel* parcel) {
  if (parcel == nullptr)
    return false;
  size_t required_object_count = objects_count_ + parcel->ObjectCount();
  if (required_object_count < objects_count_)
    return false;
  if (required_object_count >= objects_capacity_) {
    size_t new_capacity = required_object_count + 16;
    if (new_capacity < required_object_count)
      return false;
    binder_size_t* new_objects = reinterpret_cast<binder_size_t*>(
        realloc(objects_, new_capacity * sizeof(binder_size_t)));
    if (new_objects == nullptr)
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

bool Parcel::GetFdAtOffset(int* fd, size_t offset) {
  if (fd == nullptr)
    return false;
  const flat_binder_object* flat_object = GetObjectAtOffset(offset);
  if (flat_object == nullptr)
    return false;
  if (flat_object->type != BINDER_TYPE_FD)
    return false;
  *fd = flat_object->handle;
  return true;
}

bool Parcel::CopyStrongBinderAtOffsetToProtocolBuffer(size_t offset,
                                                      StrongBinder* proto) {
  CHECK(proto);
  proto->Clear();

  const flat_binder_object* flat_object = GetObjectAtOffset(offset);
  if (!flat_object)
    return false;

  switch (flat_object->type) {
    case BINDER_TYPE_BINDER:
      proto->set_host_cookie(flat_object->cookie);
      return true;
    case BINDER_TYPE_HANDLE:
      proto->set_proxy_handle(flat_object->handle);
      return true;
  }
  return false;
}

bool Parcel::InitFromBinderTransaction(
    void* data,
    size_t data_len,
    binder_size_t* objects,
    size_t objects_size,
    const ReleaseCallback& release_callback) {
  if (data_) {
    // Already allocated.
    return false;
  }
  data_ = reinterpret_cast<uint8_t*>(data);
  data_len_ = data_len;
  objects_ = objects;
  objects_count_ = objects_size / sizeof(binder_size_t);
  release_callback_ = release_callback;
  return true;
}

bool Parcel::SetCapacity(size_t capacity) {
  // Can't shink yet
  if (capacity < data_capacity_)
    return false;
  size_t new_capacity = PAD_SIZE(capacity);
  if (new_capacity < capacity)
    return false;
  uint8_t* new_data = reinterpret_cast<uint8_t*>(realloc(data_, new_capacity));
  if (new_data == nullptr)
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
  if (pos > data_len_)
    return false;
  data_pos_ = pos;
  return true;
}

// ---- Private API ----

template <class T>
bool Parcel::WriteAligned(T val) {
  // TODO(leecam): compile time check for alignment.
  if (data_pos_ + sizeof(val) < data_pos_)
    return false;
  if (data_pos_ + sizeof(val) > data_capacity_) {
    // Grow the backing buffer.
    if (!Grow(sizeof(val)))
      return false;
  }
  *reinterpret_cast<T*>(data_ + data_pos_) = val;
  AdvancePostion(sizeof(val));
  return true;
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
    // Need to grow objects.
    // TODO(leecam): Is this growth strategy any good?
    size_t new_capacity = objects_capacity_ + 16;
    if (new_capacity < objects_capacity_)
      return false;
    binder_size_t* new_objects = reinterpret_cast<binder_size_t*>(
        realloc(objects_, new_capacity * sizeof(binder_size_t)));
    if (new_objects == nullptr)
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

template <class T>
bool Parcel::ReadAligned(T* val) {
  // TODO(leecam): Compile time check for aligmment.
  if (data_pos_ + sizeof(T) < data_pos_)
    return false;
  if (val == nullptr)
    return false;
  if ((data_pos_ + sizeof(T)) <= data_len_) {
    const void* data = data_ + data_pos_;
    data_pos_ += sizeof(T);
    *val = *reinterpret_cast<const T*>(data);
    return true;
  }
  return false;
}

const flat_binder_object* Parcel::ReadObject() {
  if (data_pos_ + sizeof(flat_binder_object) < data_pos_)
    return nullptr;
  if ((data_pos_ + sizeof(flat_binder_object)) > data_len_)
    return nullptr;
  const flat_binder_object* obj =
      reinterpret_cast<const flat_binder_object*>(data_ + data_pos_);
  // TODO(leecam): Validate this object.
  data_pos_ += sizeof(flat_binder_object);
  return obj;
}

bool Parcel::Grow(size_t extra_required) {
  // Increase capacity by |extra_required| then add 50%
  // to save on repeated allocations.
  size_t new_capacity = PAD_SIZE(((data_capacity_ + extra_required) * 3) / 2);
  if (new_capacity < data_capacity_)
    return false;
  return SetCapacity(new_capacity);
}

void Parcel::AdvancePostion(size_t len) {
  // Overflow and capacity checks always done in caller.
  data_pos_ += len;
  if (data_pos_ > data_len_)
    data_len_ = data_pos_;
}

void* Parcel::AllocatePaddedBuffer(size_t len) {
  size_t padded_len = PAD_SIZE(len);
  if (padded_len < len || (data_pos_ + padded_len) < data_pos_)
    return nullptr;
  if (data_pos_ + padded_len >= data_capacity_) {
    if (!Grow(padded_len))
      return nullptr;
  }
  uint8_t* data = data_ + data_pos_;
  // If we have padding then zero the last word.
  if (padded_len > len) {
    *reinterpret_cast<uint32_t*>(data + padded_len - sizeof(uint32_t)) = 0;
  }
  AdvancePostion(padded_len);
  return data;
}

void* Parcel::GetPaddedBuffer(size_t len) {
  if ((data_pos_ + PAD_SIZE(len)) >= data_pos_ &&
      (data_pos_ + PAD_SIZE(len)) <= data_len_ && len <= PAD_SIZE(len)) {
    void* data = data_ + data_pos_;
    data_pos_ += PAD_SIZE(len);
    return data;
  }
  return nullptr;
}

const flat_binder_object* Parcel::GetObjectAtOffset(size_t offset) {
  size_t base = data_pos_ + (offset * sizeof(flat_binder_object));
  if (base < (offset * sizeof(flat_binder_object)))
    return nullptr;
  if (base + sizeof(flat_binder_object) < data_pos_)
    return nullptr;
  if ((base + sizeof(flat_binder_object)) > data_len_)
    return nullptr;
  const flat_binder_object* obj =
      reinterpret_cast<const flat_binder_object*>(data_ + base);
  // TODO(leecam): Validate this object
  return obj;
}

}  // namespace protobinder
