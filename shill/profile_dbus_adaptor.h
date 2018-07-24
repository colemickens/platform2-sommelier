// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_DBUS_ADAPTOR_H_
#define SHILL_PROFILE_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Profile.h"

namespace shill {

class Profile;

// Subclass of DBusAdaptor for Profile objects
// There is a 1:1 mapping between Profile and ProfileDBusAdaptor
// instances.  Furthermore, the Profile owns the ProfileDBusAdaptor
// and manages its lifetime, so we're OK with ProfileDBusAdaptor
// having a bare pointer to its owner profile.
//
// A Profile is a collection of Entry structures (which we will define later).
class ProfileDBusAdaptor : public org::chromium::flimflam::Profile_adaptor,
                           public DBusAdaptor,
                           public ProfileAdaptorInterface {
 public:
  static const char kPath[];

  ProfileDBusAdaptor(DBus::Connection* conn, Profile* profile);
  ~ProfileDBusAdaptor() override;

  // Implementation of ProfileAdaptorInterface.
  const std::string& GetRpcIdentifier() override { return path(); }
  void EmitBoolChanged(const std::string& name, bool value) override;
  void EmitUintChanged(const std::string& name, uint32_t value) override;
  void EmitIntChanged(const std::string& name, int value) override;
  void EmitStringChanged(const std::string& name,
                         const std::string& value) override;

  // Implementation of Profile_adaptor
  std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error& error) override;  // NOLINT
  void SetProperty(const std::string& name,
                   const DBus::Variant& value,
                   DBus::Error& error) override;  // NOLINT

  // Gets an "Entry", which is apparently a different set of properties than
  // those returned by GetProperties.
  std::map<std::string, DBus::Variant> GetEntry(
      const std::string&,
      DBus::Error& error) override;  // NOLINT

  // Deletes an Entry.
  void DeleteEntry(const std::string& , DBus::Error& error) override;  // NOLINT

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_PROFILE_DBUS_ADAPTOR_H_
