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

  // Supplicant has added a BSS to its table of visible endpoints.
  virtual void BSSAdded(const ::DBus::Path &BSS,
                        const PropertyMap &properties) = 0;

  // Supplicant has removed a BSS from its table of visible endpoints.
  virtual void BSSRemoved(const ::DBus::Path &BSS) = 0;

  // Supplicant has received a certficate from the remote server during
  // the process of authentication.
  virtual void Certification(const PropertyMap &properties) = 0;

  // Supplicant state machine has output an EAP event notification.
  virtual void EAPEvent(const std::string &status,
                        const std::string &parameter) = 0;

  // The interface element in the supplicant has changed one or more
  // properties.
  virtual void PropertiesChanged(const PropertyMap &properties) = 0;

  // A scan has completed on this interface.
  virtual void ScanDone() = 0;
};

}  // namespace shill

#endif  // SUPPLICANT_EVENT_DELEGATE_INTERFACE_H_
