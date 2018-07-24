// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CRYPTO_UTIL_PROXY_H_
#define SHILL_MOCK_CRYPTO_UTIL_PROXY_H_

#include "shill/crypto_util_proxy.h"

#include <string>
#include <vector>

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class Error;
class EventDispatcher;

class MockCryptoUtilProxy
    : public CryptoUtilProxy,
      public base::SupportsWeakPtr<MockCryptoUtilProxy> {
 public:
  explicit MockCryptoUtilProxy(EventDispatcher* dispatcher);
  ~MockCryptoUtilProxy() override;

  MOCK_METHOD9(VerifyDestination,
               bool(const std::string& certificate,
                    const std::string& public_key,
                    const std::string& nonce,
                    const std::string& signed_data,
                    const std::string& destination_udn,
                    const std::vector<uint8_t>& ssid,
                    const std::string& bssid,
                    const ResultBoolCallback& result_callback,
                    Error* error));
  MOCK_METHOD4(EncryptData, bool(const std::string& public_key,
                                 const std::string& data,
                                 const ResultStringCallback& result_callback,
                                 Error* error));

  bool RealVerifyDestination(const std::string& certificate,
                             const std::string& public_key,
                             const std::string& nonce,
                             const std::string& signed_data,
                             const std::string& destination_udn,
                             const std::vector<uint8_t>& ssid,
                             const std::string& bssid,
                             const ResultBoolCallback& result_callback,
                             Error* error);

  bool RealEncryptData(const std::string& public_key,
                       const std::string& data,
                       const ResultStringCallback& result_callback,
                       Error* error);

  // Mock methods with useful callback signatures.  You can bind these to check
  // that appropriate async callbacks are firing at expected times.
  MOCK_METHOD2(TestResultBoolCallback, void(const Error& error, bool));
  MOCK_METHOD2(TestResultStringCallback, void(const Error& error,
                                              const std::string&));
  MOCK_METHOD2(TestResultHandlerCallback, void(const std::string& result,
                                               const Error& error));
  MOCK_METHOD3(StartShimForCommand, bool(const std::string& command,
                                         const std::string& input,
                                         const StringCallback& result_handler));

  // Methods injected to permit us to call the real method implementations.
  bool RealStartShimForCommand(const std::string& command,
                               const std::string& input,
                               const StringCallback& result_handler);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCryptoUtilProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_CRYPTO_UTIL_PROXY_H_
