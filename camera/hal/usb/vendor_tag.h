/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_USB_VENDOR_TAG_H_
#define CAMERA_HAL_USB_VENDOR_TAG_H_

#include <cstdint>

#include <system/camera_metadata.h>

#include "common/vendor_tag_manager.h"

namespace cros {

const char kVendorUsbSectionName[] = "com.google.usb";

enum VendorTags : uint32_t {
  kVendorTagVendorId = kUsbHalVendorTagStart,
  kVendorTagProductId,
  kVendorTagModelName,
  kVendorTagDevicePath,
};

static_assert(kVendorTagModelName < kUsbHalVendorTagEnd,
              "The vendor tag is out-of-range.");

class VendorTagOps {
 public:
  // The static functions for filling |vendor_tag_ops|. The real implementation
  // is delegated to the instance of |VendorTagManager|.
  static int GetTagCount(const vendor_tag_ops_t*);
  static void GetAllTags(const vendor_tag_ops_t*, uint32_t* tag_array);
  static const char* GetSectionName(const vendor_tag_ops_t*, uint32_t tag);
  static const char* GetTagName(const vendor_tag_ops_t*, uint32_t tag);
  static int GetTagType(const vendor_tag_ops_t*, uint32_t tag);

 private:
  static const VendorTagManager& GetVendorTagManager();
};

}  // namespace cros

#endif  // CAMERA_HAL_USB_VENDOR_TAG_H_
