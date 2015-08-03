// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_NETWORK_H_
#define LIBWEAVE_INCLUDE_WEAVE_NETWORK_H_

#include <map>
#include <string>

#include <base/callback.h>
#include <chromeos/errors/error.h>

namespace weave {

enum class NetworkState {
  kOffline = 0,
  kFailure,
  kConnecting,
  kConnected,
};

class Network {
 public:
  // A callback that interested parties can register to be notified of
  // transitions from online to offline and vice versa.  The boolean
  // parameter will be true if we're online, and false if we're offline.
  using OnConnectionChangedCallback = base::Callback<void(bool)>;

  virtual void AddOnConnectionChangedCallback(
      const OnConnectionChangedCallback& listener) = 0;

  // Implementation should attempt to connect to the given network with the
  // given passphrase. This is accomplished by:
  // Returns false on immediate failures with some descriptive codes in |error|.
  virtual bool ConnectToService(const std::string& ssid,
                                const std::string& passphrase,
                                const base::Closure& on_success,
                                chromeos::ErrorPtr* error) = 0;

  virtual NetworkState GetConnectionState() const = 0;

  // Starts WiFi access point for wifi setup.
  virtual void EnableAccessPoint(const std::string& ssid) = 0;

  // Stops WiFi access point.
  virtual void DisableAccessPoint() = 0;

 protected:
  virtual ~Network() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_NETWORK_H_
