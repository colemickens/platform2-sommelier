// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "callback_list.h"

#include <string>

#include <base/logging.h>
#include <base/stl_util-inl.h>

using std::map;
using std::string;

namespace shill {

CallbackList::CallbackList() {}

CallbackList::~CallbackList() {
  for (CallbackMap::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    delete it->second;
  }
}

void CallbackList::Add(const string &name, Callback *callback) {
  DCHECK(!ContainsKey(callbacks_, name));
  callbacks_[name] = callback;
}

void CallbackList::Remove(const string &name) {
  DCHECK(ContainsKey(callbacks_, name));
  CallbackMap::iterator it = callbacks_.find(name);
  if (it != callbacks_.end()) {
    delete it->second;
    callbacks_.erase(it);
  }
}

bool CallbackList::Run() const {
  bool ret = true;
  for (CallbackMap::const_iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    VLOG(2) << "Running callback " << it->first;
    bool res;
    res = it->second->Run();
    VLOG(2) << "Callback " << it->first << " returned " << res;
    ret = ret && res;
  }
  return ret;
}

}  // namespace shill
