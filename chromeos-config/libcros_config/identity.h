// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_H_

#include <string>

#include <base/macros.h>

namespace base {
class FilePath;
}  // namespace base

namespace brillo {

class CrosConfigIdentity {
 public:
  CrosConfigIdentity();
  ~CrosConfigIdentity();

  // Reads the VPD identity information from the supplied VPD file.
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  //     or '/sys/firmware/vpd/ro/whitelabel_tag'.
  bool ReadVpd(const base::FilePath& vpd_file);

  // Writes out a fake VPD file for the purposes of testing.
  // @customization_id: Whitelabel name to write
  // @vpd_file_out: Returns the '/sys/firmware/vpd/ro/customization_id'-style
  //     file that was written
  // @return true if OK, false on error
  bool FakeVpd(const std::string& customization_id,
               base::FilePath* vpd_file_out);

  // Gets the VPD identitier value read via ReadVpd
  // @return VPD ID value
  const std::string& GetVpdId() const { return vpd_id_; }

 private:
  std::string vpd_id_;

  DISALLOW_COPY_AND_ASSIGN(CrosConfigIdentity);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_H_
