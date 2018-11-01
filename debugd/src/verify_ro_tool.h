// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_VERIFY_RO_TOOL_H_
#define DEBUGD_SRC_VERIFY_RO_TOOL_H_

#include <string>
#include <vector>

#include <base/macros.h>
#include <brillo/errors/error.h>

#include "debugd/src/subprocess_tool.h"

namespace debugd {

class VerifyRoTool : public SubprocessTool {
 public:
  VerifyRoTool() = default;
  ~VerifyRoTool() override = default;

  // Returns the USB-connected DUT's Cr50 RW firmware version on success or
  // an error messages if the DUT isn't available or compatible or if an error
  // happens. A normal output on success looks like
  //
  //     RW_FW_VER=0.3.10
  std::string GetGscOnUsbRWFirmwareVer();

  // Returns the USB-connected DUT's board ID on success or an error messages if
  // the DUT isn't available or compatible or if an error happens. A normal
  // output on success looks like
  //
  //     BID_TYPE=5a534b4d
  //     BID_TYPE_INV=a5acb4b2
  //     BID_FLAGS=00007f80
  //     BID_RLZ=ZSKM
  std::string GetGscOnUsbBoardID();

  // Returns the RW firmware version in the given GSC |image_file| on success or
  // an error message if an error happens. The output looks like
  //
  //     IMAGE_RW_FW_VER=0.3.10
  std::string GetGscImageRWFirmwareVer(const std::string& image_file);

  // Returns the board ID in the given GSC |image_file| on success or
  // an error message if an error happens. The output looks like
  //
  //     IMAGE_BID_STRING=01234567
  //     IMAGE_BID_MASK=00001111
  //     IMAGE_BID_FLAGS=76543210
  std::string GetGscImageBoardID(const std::string& image_file);

  // Flashes the USB-connected DUT's Cr50 FW. If it failed, at least one error
  // message will be added to |error|. Returns whether the FW flashing is
  // successful.
  bool FlashImageToGscOnUsb(
      brillo::ErrorPtr* error, const std::string& image_file);

  // Verifies AP and EC RO FW integrity of the USB-connected DUT. If it failed,
  // at least one error message will be added to |error|. Returns whether the
  // verification is successful.
  bool VerifyDeviceOnUsbROIntegrity(
      brillo::ErrorPtr* error, const std::string& ro_desc_file);

 private:
  // Reads contents of the given firmware |image_file| and gets the values of
  // |keys| from the contents. Returns lines of the "key=value" pairs, one line
  // per pair, or an error message if not all |keys| are found or if there is a
  // problem reading |image_file|.
  std::string GetKeysValuesFromImage(const std::string& image_file,
                                     const std::vector<std::string>& keys);

  DISALLOW_COPY_AND_ASSIGN(VerifyRoTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_VERIFY_RO_TOOL_H_
