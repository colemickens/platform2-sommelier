// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_ACCESSOR_
#define SHILL_PROPERTY_ACCESSOR_

#include <base/basictypes.h>
#include <base/logging.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST.

#include "shill/accessor_interface.h"
#include "shill/error.h"

namespace shill {

// Templated implementations of AccessorInterface<>.
// PropertyAccessor<> and ConstPropertyAccessor<> respectively provide
// R/W and R/O access to the value pointed to by |property|.
// this allows a class to easily map strings to member variables, so that
// pieces of state stored in the class can be queried or updated by name.
//
//   bool foo = true;
//   map<string, BoolAccessor> accessors;
//   accessors["foo"] = BoolAccessor(new PropertyAccessor<bool>(&foo));
//   bool new_foo = accessors["foo"]->Get();  // new_foo == true
//   accessors["foo"]->Set(false);  // returns true, because setting is allowed.
//                                  // foo == false, new_foo == true
//   new_foo = accessors["foo"]->Get();  // new_foo == false
template <class T>
class PropertyAccessor : public AccessorInterface<T> {
 public:
  explicit PropertyAccessor(T *property) : property_(property) {
    DCHECK(property);
  }
  virtual ~PropertyAccessor() {}

  T Get(Error */*error*/) { return *property_; }
  void Set(const T &value, Error */*error*/) {
    *property_ = value;
  }

 private:
  T * const property_;
  DISALLOW_COPY_AND_ASSIGN(PropertyAccessor);
};

template <class T>
class ConstPropertyAccessor : public AccessorInterface<T> {
 public:
  explicit ConstPropertyAccessor(const T *property) : property_(property) {
    DCHECK(property);
  }
  virtual ~ConstPropertyAccessor() {}

  T Get(Error */*error*/) { return *property_; }
  void Set(const T &/*value*/, Error *error) {
    // TODO(quiche): check if this is the right error.
    // (maybe Error::kPermissionDenied instead?)
    error->Populate(Error::kInvalidArguments, "Property is read-only");
  }

 private:
  const T * const property_;
  DISALLOW_COPY_AND_ASSIGN(ConstPropertyAccessor);
};

template <class T>
class WriteOnlyPropertyAccessor : public AccessorInterface<T> {
 public:
  explicit WriteOnlyPropertyAccessor(T *property) : property_(property) {
    DCHECK(property);
  }
  virtual ~WriteOnlyPropertyAccessor() {}

  T Get(Error *error) {
    error->Populate(Error::kPermissionDenied, "Property is write-only");
    return T();
  }
  void Set(const T &value, Error */*error*/) {
    *property_ = value;
  }

 private:
  FRIEND_TEST(PropertyAccessorTest, SignedIntCorrectness);
  FRIEND_TEST(PropertyAccessorTest, UnsignedIntCorrectness);
  FRIEND_TEST(PropertyAccessorTest, StringCorrectness);

  T * const property_;
  DISALLOW_COPY_AND_ASSIGN(WriteOnlyPropertyAccessor);
};

// CustomAccessor<> allows custom getter and setter methods to be provided.
// Thus, if the state to be returned is to be derived on-demand, we can
// still fit it into the AccessorInterface<> framework.
template<class C, class T>
class CustomAccessor : public AccessorInterface<T> {
 public:
  // |target| is the object on which to call the methods |getter| and |setter|
  // |setter| is allowed to be NULL, in which case we will simply reject
  // attempts to set via the accessor.
  // It is an error to pass NULL for either of the other two arguments.
  CustomAccessor(C *target,
                 T(C::*getter)(Error *error),
                 void(C::*setter)(const T &value, Error *error))
      : target_(target),
        getter_(getter),
        setter_(setter) {
    DCHECK(target);
  }
  virtual ~CustomAccessor() {}

  T Get(Error *error) {
    if (getter_)
      return storage_ = (target_->*getter_)(error);

    error->Populate(Error::kPermissionDenied, "Property is write-only");
    return T();
  }

  void Set(const T &value, Error *error) {
    if (setter_) {
      (target_->*setter_)(value, error);
    } else {
      error->Populate(Error::kInvalidArguments, "Property is read-only");
    }
  }

 private:
  C * const target_;
  // Get() returns a const&, so we need to have internal storage to which to
  // return a reference.
  T storage_;
  T(C::*getter_)(Error *error);
  void(C::*setter_)(const T &value, Error *error);
  DISALLOW_COPY_AND_ASSIGN(CustomAccessor);
};

}  // namespace shill

#endif  // SHILL_PROPERTY_ACCESSOR_
