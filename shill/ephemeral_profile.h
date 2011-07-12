// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_EPHEMERAL_PROFILE_
#define SHILL_EPHEMERAL_PROFILE_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>

#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;

// An in-memory profile that is not persisted to disk, but allows the
// promotion of entries contained herein to the currently active profile.
class EphemeralProfile : public Profile {
 public:
  EphemeralProfile(ControlInterface *control_interface,
                   GLib *glib,
                   Manager *manager);
  virtual ~EphemeralProfile();

  virtual bool MoveToActiveProfile(const std::string &name);

 private:
  Manager *manager_;
  DISALLOW_COPY_AND_ASSIGN(EphemeralProfile);
};

}  // namespace shill

#endif  // SHILL_EPHEMERAL_PROFILE_
