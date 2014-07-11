// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOBI_CROMO_PLUGIN_GOBI_MODEM_FACTORY_H_
#define GOBI_CROMO_PLUGIN_GOBI_MODEM_FACTORY_H_

namespace DBus {

class Connection;
struct Path;

}  // namespace DBus

namespace gobi {

struct DeviceElement;
class Sdk;

}  // namespace gobi

class GobiModem;

class GobiModemFactory {
 public:
  // Create a GobiModem object of the type corresponding to the
  // network technology for the specified device.
  static GobiModem* CreateModem(DBus::Connection& connection,
                                const DBus::Path& path,
                                gobi::DeviceElement& device,
                                gobi::Sdk* sdk);
};

#endif  // GOBI_CROMO_PLUGIN_GOBI_MODEM_FACTORY_H_
