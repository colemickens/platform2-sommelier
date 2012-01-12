// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEFAULT_PROFILE_
#define SHILL_DEFAULT_PROFILE_

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;

class DefaultProfile : public Profile {
 public:
  DefaultProfile(ControlInterface *control,
                 Manager *manager,
                 const FilePath &storage_path,
                 const Manager::Properties &manager_props);
  virtual ~DefaultProfile();

  // Loads global configuration into manager properties.
  virtual bool LoadManagerProperties(Manager::Properties *manager_props);

  // Persists profile information, as well as that of discovered devices
  // and bound services, to disk.
  // Returns true on success, false on failure.
  virtual bool Save();

 protected:
  // Sets |path| to the persistent store file path for the default, global
  // profile. Returns true on success, and false if unable to determine an
  // appropriate file location.
  //
  // In this implementation, |name_| is ignored.
  virtual bool GetStoragePath(FilePath *path);

 private:
  friend class DefaultProfileTest;
  FRIEND_TEST(DefaultProfileTest, GetStoragePath);
  FRIEND_TEST(DefaultProfileTest, LoadManagerProperties);
  FRIEND_TEST(DefaultProfileTest, Save);

  static const char kDefaultId[];
  static const char kStorageId[];
  static const char kStorageCheckPortalList[];
  static const char kStorageName[];
  static const char kStorageOfflineMode[];

  const FilePath storage_path_;
  const Manager::Properties &props_;

  DISALLOW_COPY_AND_ASSIGN(DefaultProfile);
};

}  // namespace shill

#endif  // SHILL_DEFAULT_PROFILE_
