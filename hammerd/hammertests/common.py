# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Common constant and device related information.'''

from __future__ import print_function

import os
import time
import subprocess

import hammerd_api

# The root path of the hammertests.
ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
IMAGE_DIR = os.path.join(ROOT_DIR, 'images')

# The board name.
BOARD_NAME = 'staff'

# Device-dependent information.
if BOARD_NAME == 'staff':
  BASE_VENDOR_ID = 0x18d1
  BASE_PRODUCT_ID = 0x502b
  BASE_BUS = 1
  BASE_PORT = 2
elif BOARD_NAME == 'hammer':
  BASE_VENDOR_ID = 0x18d1
  BASE_PRODUCT_ID = 0x5030
  BASE_BUS = 1
  BASE_PORT = 3
else:
  print('Error: unknown board: %s' % BOARD_NAME)

# Status of flash protect.
EC_FLASH_PROTECT_RO_AT_BOOT = (1 << 0)
EC_FLASH_PROTECT_RO_NOW = (1 << 1)
EC_FLASH_PROTECT_ALL_NOW = (1 << 2)
EC_FLASH_PROTECT_GPIO_ASSERTED = (1 << 3)
EC_FLASH_PROTECT_ERROR_STUCK = (1 << 4)
EC_FLASH_PROTECT_ERROR_INCONSISTENT = (1 << 5)
EC_FLASH_PROTECT_ALL_AT_BOOT = (1 << 6)
EC_FLASH_PROTECT_RW_AT_BOOT = (1 << 7)
EC_FLASH_PROTECT_RW_NOW = (1 << 8)
EC_FLASH_PROTECT_ROLLBACK_AT_BOOT = (1 << 9)
EC_FLASH_PROTECT_ROLLBACK_NOW = (1 << 10)

# Path of testing image files.
IMAGE = os.path.join(IMAGE_DIR, '%s.bin' % BOARD_NAME)
TP = '/lib/firmware/%s-touchpad.fw' % BOARD_NAME
RW_DEV = os.path.join(IMAGE_DIR, '%s.dev' % BOARD_NAME)
RW_CORRUPT_FIRST_BYTE = os.path.join(
    IMAGE_DIR, '%s_corrupt_first_byte.bin' % BOARD_NAME)
RW_CORRUPT_LAST_BYTE = os.path.join(
    IMAGE_DIR, '%s_corrupt_last_byte.bin' % BOARD_NAME)
RW_VALID = os.path.join(IMAGE_DIR, '%s.bin' % BOARD_NAME)
OLDER_IMAGE = os.path.join(IMAGE_DIR, '%solder.bin' % BOARD_NAME)
NEWER_IMAGE = os.path.join(IMAGE_DIR, '%snewer.bin' % BOARD_NAME)
# Image should not update RW
RB_LOWER = os.path.join(IMAGE_DIR, '%s.dev.rb0' % BOARD_NAME)
# Initial DUT image
RB_INITIAL = os.path.join(IMAGE_DIR, '%s.dev.rb1' % BOARD_NAME)
# Image should update RW and RB regions region
RB_HIGHER = os.path.join(IMAGE_DIR, '%s.dev.rb9' % BOARD_NAME)


# Common function.
def connect_usb(updater):
  updater.TryConnectUsb()
  assert updater.SendFirstPdu() == True, 'Error sending first PDU'
  updater.SendDone()


def sim_disconnect_connect(updater):
  print('Simulate hammer disconnect/ reconnect to reset base')
  subprocess.call('ectool gpioset PP3300_DX_BASE 0', shell=True)
  subprocess.call('ectool gpioset PP3300_DX_BASE 1', shell=True)
  updater.CloseUsb()
  # Need to give base time to be visible to lid
  time.sleep(3)


def disable_hammerd():
  print('Disabling hammerd')
  subprocess.call('mount --bind /dev/null /lib/udev/rules.d/99 hammerd.rules',
                  shell=True)
  subprocess.call('initctl restart udev', shell=True)


def enable_hammerd():
  print ('Enabling hammerd')
  subprocess.call('umount /lib/udev/rules.d/99-hammerd.rules', shell=True)
  subprocess.call('initctl restart udev', shell=True)


def reset_stay_ro(updater):
  # Send immediate reset
  updater.SendSubcommand(hammerd_api.UpdateExtraCommand.ImmediateReset)
  updater.CloseUsb()
  # Wait for base to reappear and send StayInRO
  time.sleep(0.5)
  updater.TryConnectUsb()
  updater.SendSubcommand(hammerd_api.UpdateExtraCommand.StayInRO)
  updater.SendFirstPdu()
  updater.SendDone()
  # Wait to stay in RO
  time.sleep(5)
  print('Current section after StayInRO cmd: %s' % updater.CurrentSection())
  assert updater.CurrentSection() == 0, 'Running section should be 0 (RO)'
