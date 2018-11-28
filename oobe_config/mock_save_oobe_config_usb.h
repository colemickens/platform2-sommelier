// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OOBE_CONFIG_MOCK_SAVE_OOBE_CONFIG_USB_H_
#define OOBE_CONFIG_MOCK_SAVE_OOBE_CONFIG_USB_H_

#include "oobe_config/save_oobe_config_usb.h"

#include <string>

#include <gmock/gmock.h>

namespace oobe_config {

class MockSaveOobeConfigUsb : public SaveOobeConfigUsb {
 public:
  MockSaveOobeConfigUsb(const base::FilePath& device_stateful,
                        const base::FilePath& usb_stateful,
                        const base::FilePath& device_ids_dir,
                        const base::FilePath& usb_device,
                        const base::FilePath& private_key_file,
                        const base::FilePath& public_key_file)
      : SaveOobeConfigUsb(device_stateful,
                          usb_stateful,
                          device_ids_dir,
                          usb_device,
                          private_key_file,
                          public_key_file) {}
};

}  // namespace oobe_config

#endif  // OOBE_CONFIG_MOCK_SAVE_OOBE_CONFIG_USB_H_
