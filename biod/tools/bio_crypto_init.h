// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIOD_TOOLS_BIO_CRYPTO_INIT_H_
#define BIOD_TOOLS_BIO_CRYPTO_INIT_H_

#include <chromeos/ec/ec_commands.h>

namespace biod {

// The definition must not be in bio_crypto_init.cc, otherwise the unit test
// executable would have to link bio_crypto_init.cc and its main() function.
// Also note that we are passing in biod's version instead of directly using
// FP_TEMPLATE_FORMAT_VERSION, because passing it in allows us to unit-test
// what happens if FP_TEMPLATE_FORMAT_VERSION were some other value.
bool CrosFpTemplateVersionCompatible(
    const uint32_t firmware_fp_template_format_version,
    const uint32_t biod_fp_template_format_version) {
  // We should modify the rule here when we uprev the template format version.
  switch (firmware_fp_template_format_version) {
    case 3:
    case 4:
      break;
    default:
      return false;
  }
  switch (biod_fp_template_format_version) {
    case 3:
    case 4:
      break;
    default:
      return false;
  }
  // If biod has template version 4, firmware with version 3 is still
  // compatible until we deprecate it.
  if (firmware_fp_template_format_version == 3 &&
      biod_fp_template_format_version == 4)
    return true;
  return firmware_fp_template_format_version == biod_fp_template_format_version;
}

}  // namespace biod

#endif  // BIOD_TOOLS_BIO_CRYPTO_INIT_H_
