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
#include "shill/dbus_bindings/flimflam-profile.h"

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

  ProfileDBusAdaptor(DBus::Connection *conn, Profile *profile);
  virtual ~ProfileDBusAdaptor();

  // Implementation of ProfileAdaptorInterface.
  virtual const std::string &GetRpcIdentifier() { return path(); }
  virtual void EmitBoolChanged(const std::string &name, bool value);
  virtual void EmitUintChanged(const std::string &name, uint32 value);
  virtual void EmitIntChanged(const std::string &name, int value);
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value);

  // Implementation of Profile_adaptor
  virtual std::map<std::string, ::DBus::Variant> GetProperties(
      ::DBus::Error &error);
  virtual void SetProperty(const std::string &name,
                           const ::DBus::Variant &value,
                           ::DBus::Error &error);

  // Gets an "Entry", which is apparently a different set of properties than
  // those returned by GetProperties.
  virtual std::map< std::string, ::DBus::Variant> GetEntry(
      const std::string& ,
      ::DBus::Error &error);

  // Deletes an Entry.
  virtual void DeleteEntry(const std::string& , ::DBus::Error &error);

 private:
  Profile *profile_;
  DISALLOW_COPY_AND_ASSIGN(ProfileDBusAdaptor);
};

}  // namespace shill
#endif  // SHILL_PROFILE_DBUS_ADAPTOR_H_
