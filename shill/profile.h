// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_
#define SHILL_PROFILE_

#include <map>
#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/event_dispatcher.h"
#include "shill/property_store.h"
#include "shill/refptr_types.h"

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
  enum InitStorageOption {
    kOpenExisting,
    kCreateNew,
    kCreateOrOpenExisting
  };
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
  bool InitStorage(GLib *glib,
                   InitStorageOption storage_option,
                   Error *error);

  // Remove the persisitent storage for this Profile.  It is an error to
  // do so while the underlying storage is open via InitStorage() or
  // set_storage().
  bool RemoveStorage(GLib *glib, Error *error);

  std::string GetFriendlyName();

  virtual std::string GetRpcIdentifier();

  PropertyStore *mutable_store() { return &store_; }
  const PropertyStore &store() const { return store_; }

  // Set the storage inteface.  This is used for testing purposes.  It
  // takes ownership of |storage|.
  void set_storage(StoreInterface *storage);

  // Begin managing the persistence of |service|.
  // Returns true if |service| is new to this profile and was added,
  // false if the |service| already existed.
  virtual bool AdoptService(const ServiceRefPtr &service);

  // Cease managing the persistence of the Service |service|.
  // Returns true if |service| was found and abandoned, or not found.
  // Returns false if can't be abandoned.
  virtual bool AbandonService(const ServiceRefPtr &service);

  // Clobbers persisted notion of |service| with data from |service|.
  // Returns true if |service| was found and updated, false if not found.
  virtual bool UpdateService(const ServiceRefPtr &service);

  // Ask |service| if it can configure itself from the profile.  If it can,
  // change the service to point at this profile, ask |service| to perform
  // the configuration and return true.  If not, return false.
  virtual bool ConfigureService(const ServiceRefPtr &service);

  // Allow the device to configure itself from this profile.  Returns
  // true if the device succeeded in finding its configuration.  If not,
  // return false.
  virtual bool ConfigureDevice(const DeviceRefPtr &device);

  // Remove a named entry from the profile.  This includes detaching
  // any service that uses this profile entry.
  virtual void DeleteEntry(const std::string &entry_name, Error *error);

  // Return a service configured from the given profile entry.
  virtual ServiceRefPtr GetServiceFromEntry(const std::string &entry_name,
                                            Error *error);

  // Return whether |service| can configure itself from the profile.
  bool ContainsService(const ServiceConstRefPtr &service);

  std::vector<std::string> EnumerateAvailableServices(Error *error);
  std::vector<std::string> EnumerateEntries(Error *error);

  // Clobbers persisted notion of |device| with data from |device|. Returns true
  // if |device| was found and updated, false otherwise. The base implementation
  // always returns false -- currently devices are persisted only in
  // DefaultProfile.
  virtual bool UpdateDevice(const DeviceRefPtr &device);

  // Write all in-memory state to disk via |storage_|.
  virtual bool Save();

  // Parses a profile identifier. There're two acceptable forms of the |raw|
  // identifier: "identifier" and "~user/identifier". Both "user" and
  // "identifier" must be suitable for use in a D-Bus object path. Returns true
  // on success.
  static bool ParseIdentifier(const std::string &raw, Identifier *parsed);

  // Returns whether |name| matches this Profile's |name_|.
  virtual bool MatchesIdentifier(const Identifier &name) const;

  // Returns the username component of the profile identifier.
  const std::string &GetUser() const { return name_.user; }

  // Returns a read-only copy of the backing storage of the profile.
  virtual const StoreInterface *GetConstStorage() const {
    return storage_.get();
  }

 protected:
  // Protected getters
  Manager *manager() const { return manager_; }
  StoreInterface *storage() { return storage_.get(); }

  // Sets |path| to the persistent store file path for a profile identified by
  // |name_|. Returns true on success, and false if unable to determine an
  // appropriate file location. |name_| must be a valid identifier,
  // possibly parsed and validated through Profile::ParseIdentifier.
  //
  // In the default implementation, |name_.user| cannot be empty, because
  // all regular profiles should be associated with a user.
  virtual bool GetStoragePath(FilePath *path);

 private:
  friend class ProfileAdaptorInterface;
  FRIEND_TEST(ProfileTest, DeleteEntry);
  FRIEND_TEST(ProfileTest, GetStoragePath);
  FRIEND_TEST(ProfileTest, IsValidIdentifierToken);

  static bool IsValidIdentifierToken(const std::string &token);

  void HelpRegisterDerivedStrings(
      const std::string &name,
      Strings(Profile::*get)(Error *error),
      void(Profile::*set)(const Strings&, Error *error));

  // Data members shared with subclasses via getter/setters above in the
  // protected: section
  Manager *manager_;

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
