// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLUGIN_GOBI_MODEM_FACTORY_H_
#define PLUGIN_GOBI_MODEM_FACTORY_H_


namespace DBus {
class Connection;
class Path;
}
namespace gobi {
struct DeviceElement;
class Sdk;
}
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

#endif  // PLUGIN_GOBI_MODEM_FACTORY_H_
