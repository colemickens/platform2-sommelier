// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_
#define SHILL_PROFILE_

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/key_file_store.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"
#include "shill/shill_event.h"

class FilePath;

namespace shill {

class ControlInterface;
class Error;
class GLib;
class ProfileAdaptorInterface;

class Profile {
 public:
  struct Identifier {
    std::string user;  // Empty for global.
    std::string identifier;
  };

  static const char kGlobalStorageDir[];
  static const char kUserStorageDirFormat[];

  Profile(ControlInterface *control_interface, GLib *glib);
  virtual ~Profile();

  PropertyStore *store() { return &store_; }

  // Parses a profile identifier. There're two acceptable forms of the |raw|
  // identifier: "identifier" and "~user/identifier". Both "user" and
  // "identifier" must be suitable for use in a D-Bus object path. Returns true
  // on success.
  static bool ParseIdentifier(const std::string &raw, Identifier *parsed);

  // Returns the RPC object path for a profile identified by
  // |identifier|. |identifier| must be a valid identifier, possibly parsed and
  // validated through Profile::ParseIdentifier.
  static std::string GetRpcPath(const Identifier &identifier);

  // Sets |path| to the persistent store file path for a profile identified by
  // |identifier|. Returns true on success, and false if unable to determine an
  // appropriate file location. |identifier| must be a valid identifier,
  // possibly parsed and validated through Profile::ParseIdentifier.
  static bool GetStoragePath(const Identifier &identifier, FilePath *path);

 protected:
  // Properties to be get/set via PropertyStore calls that must also be visible
  // in subclasses.
  PropertyStore store_;

 private:
  friend class ProfileAdaptorInterface;
  FRIEND_TEST(ProfileTest, IsValidIdentifierToken);

  static bool IsValidIdentifierToken(const std::string &token);

  scoped_ptr<ProfileAdaptorInterface> adaptor_;

  // Persistent store associated with the profile.
  KeyFileStore storage_;

  // Properties to be get/set via PropertyStore calls.
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(Profile);
};

}  // namespace shill

#endif  // SHILL_PROFILE_
