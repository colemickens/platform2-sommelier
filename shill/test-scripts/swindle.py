#!/usr/bin/env python
# Simulates an implementation of Flimflam to test other code against.
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import dbus.mainloop.glib
import dbus.service
import gobject

from swindle import manager
from swindle import profile

from swindle.devices import cellular
from swindle.devices import ethernet

if __name__ == '__main__':
  dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus()
  n = dbus.service.BusName('org.chromium.flimflam', bus)

  sw = manager.Manager(bus)
  p = profile.Profile(bus, 'default')
  sw.add_profile(p)
  sw.add_device(cellular.CellularDevice(sw, bus, 'cell0'))
  sw.add_device(ethernet.EthernetDevice(sw, bus, 'ether0'))

  mainloop = gobject.MainLoop()
  print 'swindle: running'
  mainloop.run()
