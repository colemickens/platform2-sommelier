#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import os

MMPROVIDERS = [ 'org.chromium', 'org.freedesktop' ]

class ModemManager(object):
    INTERFACE = 'org.freedesktop.ModemManager'
    MODEM_INTERFACE = 'org.freedesktop.ModemManager.Modem'
    SIMPLE_MODEM_INTERFACE = 'org.freedesktop.ModemManager.Modem.Simple'
    CDMA_MODEM_INTERFACE = 'org.freedesktop.ModemManager.Modem.Cdma'
    GSM_MODEM_INTERFACE = 'org.freedesktop.ModemManager.Modem.Gsm'
    GOBI_MODEM_INTERFACE = 'org.chromium.ModemManager.Modem.Gobi'
    GSM_CARD_INTERFACE = 'org.freedesktop.ModemManager.Modem.Gsm.Card'
    GSM_SMS_INTERFACE = 'org.freedesktop.ModemManager.Modem.Gsm.SMS'
    GSM_NETWORK_INTERFACE = 'org.freedesktop.ModemManager.Modem.Gsm.Network'
    PROPERTIES_INTERFACE = 'org.freedesktop.DBus.Properties'

    GSM_MODEM = 1
    CDMA_MODEM = 2

    @staticmethod
    def GetModemInterfaces():
        return [ModemManager.MODEM_INTERFACE,
                ModemManager.SIMPLE_MODEM_INTERFACE,
                ModemManager.CDMA_MODEM_INTERFACE,
                ModemManager.GSM_MODEM_INTERFACE,
                ModemManager.GOBI_MODEM_INTERFACE]

    def __init__(self, provider=None):
        self.bus = dbus.SystemBus()
        self.provider = provider or os.getenv('MMPROVIDER') or 'org.chromium'
        self.service = '%s.ModemManager' % self.provider
        self.path = '/%s/ModemManager' % (self.provider.replace('.', '/'))
        self.manager = dbus.Interface(
                self.bus.get_object(self.service, self.path),
                ModemManager.INTERFACE)

    def Modem(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.MODEM_INTERFACE)

    def SimpleModem(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.SIMPLE_MODEM_INTERFACE)

    def CdmaModem(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.CDMA_MODEM_INTERFACE)

    def GobiModem(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.GOBI_MODEM_INTERFACE)

    def GsmModem(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.GSM_MODEM_INTERFACE)

    def GsmCard(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.GSM_CARD_INTERFACE)

    def GsmSms(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.GSM_SMS_INTERFACE)

    def GsmNetwork(self, path):
        obj = self.bus.get_object(self.service, path)
        return dbus.Interface(obj, ModemManager.GSM_NETWORK_INTERFACE)

    def GetAll(self, iface, path):
        obj = self.bus.get_object(self.service, path)
        obj_iface = dbus.Interface(obj, ModemManager.PROPERTIES_INTERFACE)
        return obj_iface.GetAll(iface)

    def Properties(self, path):
        props = dict()
        for iface in ModemManager.GetModemInterfaces():
            try:
                d = self.GetAll(iface, path)
            except Exception, _:
                continue
            if d:
                for k, v in d.iteritems():
                    props[k] = v

        return props


def EnumerateDevices(manager=None):
    """ Enumerate all modems in the system

    Args:
      manager - the specific manager to use, if None check all known managers

    Returns:
      a list of (ModemManager object, modem dbus path)
    """
    if manager:
        managers = [manager]
    else:
        managers = []
        for provider in MMPROVIDERS:
            try:
                managers.append(ModemManager(provider))
            except dbus.exceptions.DBusException, e:
                if (e.get_dbus_name() !=
                    'org.freedesktop.DBus.Error.ServiceUnknown'):
                    raise

    result = []
    for m in managers:
        for path in m.manager.EnumerateDevices():
            result.append((m, path))

    return result


def PickOneModem(modem_pattern, manager=None):
    """ Pick a modem

    If a machine has a single modem, managed by one one of the
    MMPROVIDERS, return the dbus path and a ModemManager object for
    that modem.

    Args:
      modem_pattern - pattern that should match the modem path
      manager - the specific manager to use, if None check all known managers

    Returns:
      ModemManager object, modem dbus path

    Raises:
      ValueError - if there are no matching modems, or there are more
                   than one
    """
    devices = EnumerateDevices(manager)

    matches = [(m, path) for m, path in devices if modem_pattern in path]
    if not matches:
        raise ValueError("No modems had substring: " + modem_pattern)
    if len(matches) > 1:
        raise ValueError("Expected only one modem, got: " +
                         ", ".join([path for _, path in matches]))
    return matches[0]
