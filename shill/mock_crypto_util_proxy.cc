// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_crypto_util_proxy.h"

#include <string>
#include <vector>

#include <base/basictypes.h>

#include "shill/callbacks.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"

namespace shill {

MockCryptoUtilProxy::MockCryptoUtilProxy(
    EventDispatcher *dispatcher, GLib *glib)
    : CryptoUtilProxy(dispatcher, glib) {}

MockCryptoUtilProxy::~MockCryptoUtilProxy() {}

bool MockCryptoUtilProxy::RealVerifyDestination(
    const std::string &certificate,
    const std::string &public_key,
    const std::string &nonce,
    const std::string &signed_data,
    const std::string &destination_udn,
    const std::vector<uint8_t> &ssid,
    const std::string &bssid,
    const ResultBoolCallback &result_callback,
    Error *error) {
  return CryptoUtilProxy::VerifyDestination(certificate, public_key,
                                            nonce,signed_data,
                                            destination_udn, ssid, bssid,
                                            result_callback, error);
}

bool MockCryptoUtilProxy::RealEncryptData(
    const std::string &public_key,
    const std::string &data,
    const ResultStringCallback &result_callback,
    Error *error) {
  return CryptoUtilProxy::EncryptData(public_key, data,
                                      result_callback, error);
}

bool MockCryptoUtilProxy::RealStartShimForCommand(
    const std::string &command,
    const std::string &input,
    const StringCallback &result_handler) {
  return CryptoUtilProxy::StartShimForCommand(command, input,
                                              result_handler);
}

}  // namespace shill
