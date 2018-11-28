// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OOBE_CONFIG_MOCK_LOAD_OOBE_CONFIG_USB_H_
#define OOBE_CONFIG_MOCK_LOAD_OOBE_CONFIG_USB_H_

#include "oobe_config/load_oobe_config_usb.h"

#include <string>

#include <gmock/gmock.h>

namespace oobe_config {

class MockLoadOobeConfigUsb : public LoadOobeConfigUsb {
 public:
  MockLoadOobeConfigUsb(const base::FilePath& stateful_dir,
                        const base::FilePath& device_ids_dir,
                        const base::FilePath& store_dir)
      : LoadOobeConfigUsb(stateful_dir, device_ids_dir, store_dir) {}

  MOCK_METHOD0(VerifyPublicKey, bool(void));
  MOCK_METHOD2(MountUsbDevice,
               bool(const base::FilePath&, const base::FilePath&));
  MOCK_METHOD1(UnmountUsbDevice, bool(const base::FilePath&));
};

}  // namespace oobe_config

#endif  // OOBE_CONFIG_MOCK_LOAD_OOBE_CONFIG_USB_H_
