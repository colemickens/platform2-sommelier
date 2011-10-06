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
class StoreInterface;

class Profile : public base::RefCounted<Profile> {
 public:
  struct Identifier {
    Identifier() {}
    explicit Identifier(const std::string &i) : identifier(i) {}
    Identifier(const std::string &u, const std::string &i)
        : user(u),
          identifier(i) {
    }
    std::string user;  // Empty for global.
    std::string identifier;
  };

  Profile(ControlInterface *control_interface,
          Manager *manager,
          const Identifier &name,
          const std::string &user_storage_format,
          bool connect_to_rpc);

  virtual ~Profile();

  // Set up persistent storage for this Profile.
  bool InitStorage(GLib *glib);

  std::string GetFriendlyName();

  std::string GetRpcIdentifier();

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }

  void set_storage(StoreInterface *storage);  // Takes ownership of |storage|.

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

  // Write all in-memory state to disk via |storage_|.
  virtual bool Save();

  // Sets |path| to the persistent store file path for a profile identified by
  // |name_|. Returns true on success, and false if unable to determine an
  // appropriate file location. |name_| must be a valid identifier,
  // possibly parsed and validated through Profile::ParseIdentifier.
  //
  // In the default implementation, |name_.user| cannot be empty.
  virtual bool GetStoragePath(FilePath *path);

 protected:
  // Protected getters
  Manager *manager() const { return manager_; }
  std::map<std::string, ServiceRefPtr> *services() { return &services_; }
  StoreInterface *storage() { return storage_.get(); }

 private:
  friend class ProfileAdaptorInterface;
  FRIEND_TEST(ProfileTest, IsValidIdentifierToken);
  FRIEND_TEST(ProfileTest, ParseIdentifier);
  // TODO(cmasone): once we can add services organically, take this out.
  FRIEND_TEST(ServiceTest, MoveService);

  static bool IsValidIdentifierToken(const std::string &token);

  // Parses a profile identifier. There're two acceptable forms of the |raw|
  // identifier: "identifier" and "~user/identifier". Both "user" and
  // "identifier" must be suitable for use in a D-Bus object path. Returns true
  // on success.
  static bool ParseIdentifier(const std::string &raw, Identifier *parsed);

  // Returns true if |candidate| can be merged into |service|.
  bool Mergeable(const ServiceRefPtr &/*service*/,
                 const ServiceRefPtr &/*candiate*/) {
    return false;
  }

  void HelpRegisterDerivedStrings(
      const std::string &name,
      Strings(Profile::*get)(void),
      void(Profile::*set)(const Strings&, Error *));

  // Persists |services_| to disk via |storage_|.
  bool SaveServices();

  // Data members shared with subclasses via getter/setters above in the
  // protected: section
  Manager *manager_;
  std::map<std::string, ServiceRefPtr> services_;

  // Shared with |adaptor_| via public getter.
  PropertyStore store_;

  // Properties to be gotten via PropertyStore calls.
  Identifier name_;

  // Format string used to generate paths to user profile directories.
  const std::string storage_format_;

  // Allows this profile to be backed with on-disk storage.
  scoped_ptr<StoreInterface> storage_;

  scoped_ptr<ProfileAdaptorInterface> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Profile);
};

}  // namespace shill

#endif  // SHILL_PROFILE_
