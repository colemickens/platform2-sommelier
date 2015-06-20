// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_PRIVET_PRIVETD_CONF_PARSER_H_
#define BUFFET_PRIVET_PRIVETD_CONF_PARSER_H_

#include <set>
#include <string>

#include <chromeos/key_value_store.h>

namespace privetd {

enum class PairingType;

class PrivetdConfigParser final {
 public:
  PrivetdConfigParser();

  bool Parse(const chromeos::KeyValueStore& config_store);

  bool wifi_auto_setup_enabled() const { return wifi_auto_setup_enabled_; }
  const std::set<PairingType>& pairing_modes() { return pairing_modes_; }
  const base::FilePath& embedded_code_path() const {
    return embedded_code_path_;
  }

 private:
  bool wifi_auto_setup_enabled_;
  std::set<PairingType> pairing_modes_;
  base::FilePath embedded_code_path_;
};

}  // namespace privetd

#endif  // BUFFET_PRIVET_PRIVETD_CONF_PARSER_H_
