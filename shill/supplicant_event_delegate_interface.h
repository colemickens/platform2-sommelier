// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SUPPLICANT_EVENT_DELEGATE_INTERFACE_H_
#define SUPPLICANT_EVENT_DELEGATE_INTERFACE_H_

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

namespace shill {

// SupplicantEventDelegateInterface declares the set of methods that
// a SupplicantInterfaceProxy calls on an interested party when
// wpa_supplicant events occur on the network interface interface.
class SupplicantEventDelegateInterface {
 public:
  typedef std::map<std::string, ::DBus::Variant> PropertyMap;

  virtual ~SupplicantEventDelegateInterface() {}

  virtual void BSSAdded(const ::DBus::Path &BSS,
                        const PropertyMap &properties) = 0;
  virtual void BSSRemoved(const ::DBus::Path &BSS) = 0;
  virtual void Certification(const PropertyMap &properties) = 0;
  virtual void EAPEvent(const std::string &status,
                        const std::string &parameter) = 0;
  virtual void PropertiesChanged(const PropertyMap &properties) = 0;
  virtual void ScanDone() = 0;
};

}  // namespace shill

#endif  // SUPPLICANT_EVENT_DELEGATE_INTERFACE_H_
