// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEFAULT_PROFILE_
#define SHILL_DEFAULT_PROFILE_

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>

#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

namespace shill {

class ControlInterface;

class DefaultProfile : public Profile {
 public:
  DefaultProfile(ControlInterface *control,
                 GLib *glib,
                 Manager *manager,
                 const FilePath &storage_path,
                 const Manager::Properties &manager_props);
  virtual ~DefaultProfile();


  // Sets |path| to the persistent store file path for the default, global
  // profile. Returns true on success, and false if unable to determine an
  // appropriate file location.
  //
  // In this implementation, |name_| is ignored.
  virtual bool GetStoragePath(FilePath *path);

 private:
  static const char kDefaultId[];

  const FilePath storage_path_;

  DISALLOW_COPY_AND_ASSIGN(DefaultProfile);
};

}  // namespace shill

#endif  // SHILL_DEFAULT_PROFILE_
