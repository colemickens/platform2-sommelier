// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_MOCK_MOJO_CLIENT_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_MOCK_MOJO_CLIENT_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mojo/public/cpp/system/buffer.h>

#include "mojo/wilco_dtc_supportd.mojom.h"

namespace diagnostics {

class MockMojoClient
    : public chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdClient {
 public:
  using MojoWilcoDtcSupportdWebRequestHttpMethod =
      chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestHttpMethod;
  using MojoWilcoDtcSupportdWebRequestStatus =
      chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdWebRequestStatus;
  using MojoWilcoDtcSupportdEvent =
      chromeos::wilco_dtc_supportd::mojom::WilcoDtcSupportdEvent;
  using MojoPerformWebRequestCallback = base::Callback<void(
      MojoWilcoDtcSupportdWebRequestStatus, int, mojo::ScopedHandle)>;
  using MojoGetConfigurationDataCallback =
      base::Callback<void(const std::string&)>;

  void SendWilcoDtcMessageToUi(
      mojo::ScopedHandle json_message,
      const SendWilcoDtcMessageToUiCallback& callback) override;

  void PerformWebRequest(
      MojoWilcoDtcSupportdWebRequestHttpMethod http_method,
      mojo::ScopedHandle url,
      std::vector<mojo::ScopedHandle> headers,
      mojo::ScopedHandle request_body,
      const MojoPerformWebRequestCallback& callback) override;

  MOCK_METHOD(void,
              SendWilcoDtcMessageToUiImpl,
              (const std::string&, const SendWilcoDtcMessageToUiCallback&));
  MOCK_METHOD(void,
              PerformWebRequestImpl,
              (MojoWilcoDtcSupportdWebRequestHttpMethod,
               const std::string&,
               const std::vector<std::string>&,
               const std::string&,
               const MojoPerformWebRequestCallback&));
  MOCK_METHOD(void,
              GetConfigurationData,
              (const MojoGetConfigurationDataCallback&),
              (override));
  MOCK_METHOD(void, HandleEvent, (const MojoWilcoDtcSupportdEvent), (override));
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_MOCK_MOJO_CLIENT_H_
