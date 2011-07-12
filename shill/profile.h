// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_
#define SHILL_PROFILE_

#include <map>
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
class Manager;
class ProfileAdaptorInterface;

class Profile : public base::RefCounted<Profile> {
 public:
  struct Identifier {
    std::string user;  // Empty for global.
    std::string identifier;
  };

  static const char kGlobalStorageDir[];
  static const char kUserStorageDirFormat[];

  Profile(ControlInterface *control_interface, GLib *glib, Manager *manager);
  virtual ~Profile();

  const std::string &name() { return name_; }

  PropertyStore *store() { return &store_; }

  // Begin managing the persistence of |service|.
  // Returns true if |service| is new to this profile and was added,
  // false if the |service| already existed.
  bool AdoptService(const ServiceRefPtr &service);

  // Cease managing the persistence of the Service named |name|.
  // Returns true if |name| was found and abandoned, false if not found.
  bool AbandonService(const std::string &name);

  // Continue persisting the Service named |name|, but don't consider it
  // usable for connectivity.
  // Returns true if |name| was found and demoted, false if not found.
  bool DemoteService(const std::string &name);

  // Determine if |service| represents a service that's already in |services_|.
  // If so, merge them smartly and return true.  If not, return false.
  bool MergeService(const ServiceRefPtr &service);

  ServiceRefPtr FindService(const std::string& name);

  std::vector<std::string> EnumerateAvailableServices();
  std::vector<std::string> EnumerateEntries();

  // Flush any pending entry info to disk and stop managing service persistence.
  virtual void Finalize();

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
  Manager *manager_;

  // Properties to be get/set via PropertyStore calls that must also be visible
  // in subclasses.
  PropertyStore store_;

  std::map<std::string, ServiceRefPtr> services_;

 private:
  friend class ProfileAdaptorInterface;
  FRIEND_TEST(ProfileTest, IsValidIdentifierToken);
  // TODO(cmasone): once we can add services organically, take this out.
  FRIEND_TEST(ServiceTest, MoveService);

  static bool IsValidIdentifierToken(const std::string &token);

  // Returns true if |candidate| can be merged into |service|.
  bool Mergeable(const ServiceRefPtr &service, const ServiceRefPtr &candiate) {
    return false;
  }

  void HelpRegisterDerivedStrings(const std::string &name,
                                  Strings(Profile::*get)(void),
                                  bool(Profile::*set)(const Strings&));

  scoped_ptr<ProfileAdaptorInterface> adaptor_;

  // Persistent store associated with the profile.
  KeyFileStore storage_;

  // Properties to be get/set via PropertyStore calls.
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(Profile);
};

}  // namespace shill

#endif  // SHILL_PROFILE_
