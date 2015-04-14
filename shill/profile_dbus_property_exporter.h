// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROFILE_DBUS_PROPERTY_EXPORTER_H_
#define SHILL_PROFILE_DBUS_PROPERTY_EXPORTER_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace shill {

class Error;
class StoreInterface;

// This class is responsible for loading stored profile properties
// from storage for presentation using the Profile::GetEntry DBus
// API.  Properties are loaded and presented in much the same way
// as a live service would present them.  This is troublesome
// because it needs to duplicate (and stay in sync with) the way
// properties are loaded and presented in a real service.
//
// TODO(pstew): Get rid of this.  It's nasty.  crbug.com/208736
class ProfileDBusPropertyExporter {
 public:
  typedef std::map<std::string, ::DBus::Variant> PropertyList;

  ProfileDBusPropertyExporter(const StoreInterface *storage,
                              const std::string &entry_name);
  virtual ~ProfileDBusPropertyExporter();

  bool LoadServiceProperties(PropertyList *properties,
                             Error *error);

 private:
#if !defined(DISABLE_WIFI) || !defined(DISABLE_WIRED_8021X)
  bool LoadEapServiceProperties(PropertyList *properties,
                                Error *error);
#endif  // DISABLE_WIFI || DISABLE_WIRED_8021X

#if !defined(DISABLE_WIFI)
  bool LoadWiFiServiceProperties(PropertyList *properties,
                                 Error *error);
#endif  // DISABLE_WIFI

  bool LoadBool(PropertyList *properties,
                const std::string &storage_name,
                const std::string &dbus_name);
  bool LoadString(PropertyList *properties,
                  const std::string &storage_name,
                  const std::string &dbus_name);
  void SetBool(PropertyList *properties,
               const std::string &dbus_name,
               bool value);
  void SetString(PropertyList *properties,
                 const std::string &dbus_name,
                 const std::string &value);

  const StoreInterface *storage_;
  const std::string entry_name_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDBusPropertyExporter);
};

}  // namespace shill

#endif  // SHILL_PROFILE_DBUS_PROPERTY_EXPORTER_H_
