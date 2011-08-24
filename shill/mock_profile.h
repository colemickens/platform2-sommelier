// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PROFILE_
#define SHILL_MOCK_PROFILE_

#include <string>

#include <gmock/gmock.h>

#include "shill/profile.h"


namespace shill {

class ControlInterface;
class GLib;
class Manager;

class MockProfile : public Profile {
 public:
  MockProfile(ControlInterface *control, GLib *glib, Manager *manager);
  MockProfile(ControlInterface *control,
              GLib *glib,
              Manager *manager,
              const std::string &identifier);
  virtual ~MockProfile();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProfile);
};

}  // namespace shill

#endif  // SHILL_MOCK_PROFILE_
