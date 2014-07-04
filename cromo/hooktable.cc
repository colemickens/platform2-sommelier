// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "cromo/hooktable.h"

HookTable::Hook::Hook(Function func, void* arg) : func_(func), arg_(arg) {}

HookTable::Hook::~Hook() {}

HookTable::HookTable() {}
HookTable::~HookTable() {}

void HookTable::Add(const std::string& name, Function func, void* arg) {
  Hook* h = new Hook(func, arg);
  hooks_[name] = h;
}

void HookTable::Del(const std::string& name) {
  CHECK(hooks_.find(name) != hooks_.end());
  delete hooks_[name];
  hooks_.erase(name);
}

bool HookTable::Run() {
  bool retval = true;

  for (HookMap::iterator it = hooks_.begin(); it != hooks_.end(); it++) {
    LOG(INFO) << "hooktable: start " << it->first;
    Hook* hook = it->second;
    bool res = hook->func_(hook->arg_);
    retval = retval && res;
    LOG(INFO) << "hooktable: end " << it->first << " " << res;
  }

  return retval;
}
