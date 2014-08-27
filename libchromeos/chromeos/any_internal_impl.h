// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Internal implementation of chromeos::Any class.

#ifndef LIBCHROMEOS_CHROMEOS_ANY_INTERNAL_IMPL_H_
#define LIBCHROMEOS_CHROMEOS_ANY_INTERNAL_IMPL_H_

#include <type_traits>
#include <typeinfo>
#include <utility>

#include <base/logging.h>
#include <chromeos/dbus/data_serialization.h>

namespace chromeos {

namespace internal_details {

// An extension to std::is_convertible to allow conversion from an enum to
// an integral type which std::is_convertible does not indicate as supported.
template <typename From, typename To>
struct IsConvertible : public std::integral_constant<bool,
    std::is_convertible<From, To>::value ||
    (std::is_enum<From>::value && std::is_integral<To>::value)> {
};
// TryConvert is a helper function that does a safe compile-time conditional
// type cast between data types that may not be always convertible.
// From and To are the source and destination types.
// The function returns true if conversion was possible/successful.
template <typename From, typename To>
inline typename std::enable_if<IsConvertible<From, To>::value, bool>::type
TryConvert(const From& in, To* out) {
  *out = static_cast<To>(in);
  return true;
}
template <typename From, typename To>
inline typename std::enable_if<!IsConvertible<From, To>::value, bool>::type
TryConvert(const From& in, To* out) {
  return false;
}

class Buffer;  // Forward declaration of data buffer container.

// Abstract base class for contained variant data.
struct Data {
  virtual ~Data() {}
  // Returns the type information for the contained data.
  virtual const std::type_info& GetType() const = 0;
  // Copies the contained data to the output |buffer|.
  virtual void CopyTo(Buffer* buffer) const = 0;
  // Moves the contained data to the output |buffer|.
  virtual void MoveTo(Buffer* buffer) = 0;
  // Checks if the contained data is an integer type (not necessarily an 'int').
  virtual bool IsConvertibleToInteger() const = 0;
  // Gets the contained integral value as an integer.
  virtual intmax_t GetAsInteger() const = 0;
  // Writes the contained value to the D-Bus message buffer.
  virtual bool AppendToDBusMessage(dbus::MessageWriter* writer) const = 0;
};

// Concrete implementation of variant data of type T.
template<typename T>
struct TypedData : public Data {
  explicit TypedData(const T& value) : value_(value) {}
  // NOLINTNEXTLINE(build/c++11)
  explicit TypedData(T&& value) : value_(std::move(value)) {}

  const std::type_info& GetType() const override { return typeid(T); }
  void CopyTo(Buffer* buffer) const override;
  void MoveTo(Buffer* buffer) override;
  bool IsConvertibleToInteger() const override {
    return std::is_integral<T>::value || std::is_enum<T>::value;
  }
  intmax_t GetAsInteger() const override {
    intmax_t int_val = 0;
    bool converted = TryConvert(value_, &int_val);
    CHECK(converted) << "Unable to convert value of type " << typeid(T).name()
                     << " to integer";
    return int_val;
  }
  bool AppendToDBusMessage(dbus::MessageWriter* writer) const override {
    return chromeos::dbus_utils::AppendValueToWriterAsVariant(writer, value_);
  }
  // Special methods to copy/move data of the same type
  // without reallocating the buffer.
  void FastAssign(const T& source) { value_ = source; }
  // NOLINTNEXTLINE(build/c++11)
  void FastAssign(T&& source) { value_ = std::move(source); }

  T value_;
};

// Buffer class that stores the contained variant data.
// To improve performance and reduce memory fragmentation, small variants
// are stored in pre-allocated memory buffers that are part of the Any class.
// If the memory requirements are larger than the set limit or the type is
// non-trivially copyable, then the contained class is allocated in a separate
// memory block and the pointer to that memory is contained within this memory
// buffer class.
class Buffer {
 public:
  enum StorageType { kExternal, kContained };
  Buffer() : external_ptr_(nullptr), storage_(kExternal) {}
  ~Buffer() {
    Clear();
  }

  Buffer(const Buffer& rhs) : Buffer() {
    rhs.CopyTo(this);
  }
  // NOLINTNEXTLINE(build/c++11)
  Buffer(Buffer&& rhs) : Buffer() {
    rhs.MoveTo(this);
  }
  Buffer& operator=(const Buffer& rhs) {
    rhs.CopyTo(this);
    return *this;
  }
  // NOLINTNEXTLINE(build/c++11)
  Buffer& operator=(Buffer&& rhs) {
    rhs.MoveTo(this);
    return *this;
  }

  // Returns the underlying pointer to contained data. Uses either the pointer
  // or the raw data depending on |storage_| type.
  inline Data* GetDataPtr() {
    return (storage_ == kExternal) ?
        external_ptr_ : reinterpret_cast<Data*>(contained_buffer_);
  }
  inline const Data* GetDataPtr() const {
    return (storage_ == kExternal) ?
        external_ptr_ : reinterpret_cast<const Data*>(contained_buffer_);
  }

  // Destroys the contained object (and frees memory if needed).
  void Clear() {
    Data* data = GetDataPtr();
    if (storage_ == kExternal) {
      delete data;
    } else {
      // Call the destructor manually, since the object was constructed inline
      // in the pre-allocated buffer. We still need to call the destructor
      // to free any associated resources, but we can't call delete |data| here.
      data->~Data();
    }
    external_ptr_ = nullptr;
    storage_ = kExternal;
  }

  // Stores a value of type T.
  template<typename T>
  void Assign(T&& value) {  // NOLINT(build/c++11)
    using Type = typename std::decay<T>::type;
    using DataType = TypedData<Type>;
    Data* ptr = GetDataPtr();
    if (ptr && ptr->GetType() == typeid(Type)) {
      // We assign the data to the variant container, which already
      // has the data of the same type. Do fast copy/move with no memory
      // reallocation.
      DataType* typed_ptr = static_cast<DataType*>(ptr);
      // NOLINTNEXTLINE(build/c++11)
      typed_ptr->FastAssign(std::forward<T>(value));
    } else {
      Clear();
      // TODO(avakulenko): [see crbug.com/379833]
      // Unfortunately, GCC doesn't support std::is_trivially_copyable<T> yet,
      // so using std::is_trivial instead, which is a bit more restrictive.
      // Once GCC has support for is_trivially_copyable, update the following.
      if (!std::is_trivial<Type>::value ||
          sizeof(DataType) > sizeof(contained_buffer_)) {
        // If it is too big or not trivially copyable, allocate it separately.
        // NOLINTNEXTLINE(build/c++11)
        external_ptr_ = new DataType(std::forward<T>(value));
        storage_ = kExternal;
      } else {
        // Otherwise just use the pre-allocated buffer.
        DataType* address = reinterpret_cast<DataType*>(contained_buffer_);
        // Make sure we still call the copy/move constructor.
        // Call the constructor manually by using placement 'new'.
        // NOLINTNEXTLINE(build/c++11)
        new (address) DataType(std::forward<T>(value));
        storage_ = kContained;
      }
    }
  }

  // Helper methods to retrieve a reference to contained data.
  // These assume that type checking has already been performed by Any
  // so the type cast is valid and will succeed.
  template<typename T>
  const T& GetData() const {
    using DataType = internal_details::TypedData<typename std::decay<T>::type>;
    return static_cast<const DataType*>(GetDataPtr())->value_;
  }
  template<typename T>
  T& GetData() {
    using DataType = internal_details::TypedData<typename std::decay<T>::type>;
    return static_cast<DataType*>(GetDataPtr())->value_;
  }

  // Returns true if the buffer has no contained data.
  bool IsEmpty() const {
    return (storage_ == kExternal && external_ptr_ == nullptr);
  }

  // Copies the data from the current buffer into the |destination|.
  void CopyTo(Buffer* destination) const {
    if (IsEmpty()) {
      destination->Clear();
    } else {
      GetDataPtr()->CopyTo(destination);
    }
  }

  // Moves the data from the current buffer into the |destination|.
  void MoveTo(Buffer* destination) {
    if (IsEmpty()) {
      destination->Clear();
    } else {
      if (storage_ == kExternal) {
        destination->Clear();
        destination->storage_ = kExternal;
        destination->external_ptr_ = external_ptr_;
        external_ptr_ = nullptr;
      } else {
        GetDataPtr()->MoveTo(destination);
      }
    }
  }

  union {
    // |external_ptr_| is a pointer to a larger object allocated in
    // a separate memory block.
    Data* external_ptr_;
    // |contained_buffer_| is a pre-allocated buffer for smaller/simple objects.
    // Pre-allocate enough memory to store objects as big as "double".
    unsigned char contained_buffer_[sizeof(TypedData<double>)];
  };
  // Depending on a value of |storage_|, either |external_ptr_| or
  // |contained_buffer_| above is used to get a pointer to memory containing
  // the variant data.
  StorageType storage_;  // Declare after the union to eliminate member padding.
};

template<typename T>
void TypedData<T>::CopyTo(Buffer* buffer) const { buffer->Assign(value_); }
template<typename T>
void TypedData<T>::MoveTo(Buffer* buffer) { buffer->Assign(std::move(value_)); }

}  // namespace internal_details

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_ANY_INTERNAL_IMPL_H_

