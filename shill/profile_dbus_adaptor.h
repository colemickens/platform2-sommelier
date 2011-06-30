// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_DBUS_ADAPTOR_H_
#define SHILL_PROFILE_DBUS_ADAPTOR_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/flimflam-profile.h"

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
  static const char kInterfaceName[];
  static const char kPath[];

  ProfileDBusAdaptor(DBus::Connection *conn, Profile *profile);
  virtual ~ProfileDBusAdaptor();

  // Implementation of ProfileAdaptorInterface.
  void EmitBoolChanged(const std::string &name, bool value);
  void EmitUintChanged(const std::string &name, uint32 value);
  void EmitIntChanged(const std::string &name, int value);
  void EmitStringChanged(const std::string &name, const std::string &value);
  void EmitStateChanged(const std::string &new_state);

  // Implementation of Profile_adaptor
  std::map<std::string, ::DBus::Variant> GetProperties(::DBus::Error &error);
  void SetProperty(const std::string &name,
                   const ::DBus::Variant &value,
                   ::DBus::Error &error);

  // Gets an "Entry", which is apparently a different set of properties than
  // those returned by GetProperties.
  std::map< std::string, ::DBus::Variant> GetEntry(const std::string& ,
                                                   ::DBus::Error &error);

  // Deletes an Entry.
  void DeleteEntry(const std::string& , ::DBus::Error &error);

 private:
  Profile *profile_;
  DISALLOW_COPY_AND_ASSIGN(ProfileDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_PROFILE_DBUS_ADAPTOR_H_
