// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CALLBACK_LIST_
#define SHILL_CALLBACK_LIST_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/callback_old.h>
#include <base/scoped_ptr.h>

namespace shill {

typedef CallbackWithReturnValue<bool>::Type Callback;

class CallbackList {
 public:
  CallbackList();
  virtual ~CallbackList();

  void Add(const std::string &name, Callback *callback);  // takes ownership
  void Remove(const std::string &name);
  // Run all callbacks, returning false if any of the callbacks return false
  // (and returning true otherwise). The order in which the callbacks run
  // is unspecified.
  bool Run() const;

 private:
  // Can't use scoped_ptr, because it is non-copyable.
  typedef std::map<const std::string, Callback *> CallbackMap;
  CallbackMap callbacks_;

  DISALLOW_COPY_AND_ASSIGN(CallbackList);
};

}  // namespace shill

#endif  // SHILL_CALLBACK_LIST_
