// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_PRIVET_H_
#define LIBWEAVE_INCLUDE_WEAVE_PRIVET_H_

#include <string>
#include <vector>

#include <base/callback.h>

namespace weave {

enum class PairingType {
  kPinCode,
  kEmbeddedCode,
  kUltrasound32,
  kAudible32,
};

enum class WifiSetupState {
  kDisabled,
  kBootstrapping,
  kMonitoring,
  kConnecting,
};

class Privet {
 public:
  using OnWifiSetupChangedCallback = base::Callback<void(WifiSetupState state)>;
  using OnPairingStartedCallback =
      base::Callback<void(const std::string& session_id,
                          PairingType pairing_type,
                          const std::vector<uint8_t>& code)>;
  using OnPairingEndedCallback =
      base::Callback<void(const std::string& session_id)>;

  // Sets callback which is called when WiFi state is changed.
  virtual void AddOnWifiSetupChangedCallback(
      const OnWifiSetupChangedCallback& callback) = 0;

  virtual void AddOnPairingChangedCallbacks(
      const OnPairingStartedCallback& on_start,
      const OnPairingEndedCallback& on_end) = 0;

 protected:
  virtual ~Privet() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_PRIVET_H_
