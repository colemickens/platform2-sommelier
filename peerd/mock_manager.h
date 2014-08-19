// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MOCK_MANAGER_H_
#define PEERD_MOCK_MANAGER_H_

#include <dbus/bus.h>

#include "peerd/manager.h"

namespace peerd {

class MockManager : public Manager {
 public:
  explicit MockManager(const scoped_refptr<dbus::Bus>& bus) : Manager(bus) {}
};

}  // namespace peerd

#endif  // PEERD_MOCK_MANAGER_H_
