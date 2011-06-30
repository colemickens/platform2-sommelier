// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_
#define SHILL_PROFILE_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>

#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;
class Error;
class ProfileAdaptorInterface;

class Profile : public PropertyStore {
 public:
  explicit Profile(ControlInterface *control_interface);
  virtual ~Profile();

  // Implementation of PropertyStore
  virtual bool SetBoolProperty(const std::string &name,
                               bool value,
                               Error *error);
  virtual bool SetStringProperty(const std::string &name,
                                 const std::string &value,
                                 Error *error);

 private:
  scoped_ptr<ProfileAdaptorInterface> adaptor_;

  // Properties to be get/set via PropertyStore calls.

  friend class ProfileAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_PROFILE_
