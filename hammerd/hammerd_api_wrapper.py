#!/usr/bin/env python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The python wrapper of the hammerd API."""

from __future__ import print_function

import ctypes


class UpdateExtraCommand(object):
  """The enumeration of extra vendor subcommands."""
  ImmediateReset = 0
  JumpToRW = 1
  StayInRO = 2
  UnlockRW = 3
  UnlockRollback = 4
  InjectEntropy = 5
  PairChallenge = 6


class SectionName(object):
  """The enumeration of the image sections."""
  RO = 0
  RW = 1
  Invalid = 2


class FirmwareUpdater(object):
  """The wrapper of FirmwareUpdater class."""
  DLL = ctypes.CDLL('libhammerd-api.so')
  print('Hammerd API library loaded')

  def __init__(self, vendor_id, product_id, bus=-1, port=-1):
    print('Create a firmware updater')
    func = self.DLL.FirmwareUpdater_New
    func.argtypes = [ctypes.c_uint16, ctypes.c_uint16,
                     ctypes.c_int, ctypes.c_int]
    func.restype = ctypes.c_void_p
    self.updater = func(vendor_id, product_id, bus, port)

  def TryConnectUSB(self):
    print('Call TryConnectUSB')
    func = self.DLL.FirmwareUpdater_TryConnectUSB
    func.argtypes = [ctypes.c_voidp]
    func.restype = ctypes.c_bool
    return func(self.updater)

  def CloseUSB(self):
    print('Call CloseUSB')
    func = self.DLL.FirmwareUpdater_CloseUSB
    func.argtypes = [ctypes.c_voidp]
    func.restype = None
    return func(self.updater)

  def SendSubcommand(self, subcommand):
    print('Call SendSubcommand')
    func = self.DLL.FirmwareUpdater_SendSubcommand
    func.argtypes = [ctypes.c_voidp, ctypes.c_uint16]
    func.restype = ctypes.c_bool
    return func(self.updater, subcommand)


def main():
  """The demonstration of FirmwareUpdater usage."""
  updater = FirmwareUpdater(0x18d1, 0x5022)
  updater.TryConnectUSB()
  updater.SendSubcommand(UpdateExtraCommand.ImmediateReset)
  updater.CloseUSB()


if __name__ == '__main__':
  main()
