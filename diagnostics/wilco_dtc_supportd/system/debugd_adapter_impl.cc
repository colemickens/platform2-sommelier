// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/wilco_dtc_supportd/system/debugd_adapter_impl.h"

#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <brillo/errors/error.h>

namespace diagnostics {

namespace {

constexpr char kSmartAttributesOption[] = "attributes";

}  // namespace

DebugdAdapterImpl::DebugdAdapterImpl(
    std::unique_ptr<org::chromium::debugdProxyInterface> debugd_proxy)
    : debugd_proxy_(std::move(debugd_proxy)) {
  DCHECK(debugd_proxy_);
}

DebugdAdapterImpl::~DebugdAdapterImpl() = default;

void DebugdAdapterImpl::GetSmartAttributes(
    const SmartAttributesCallback& callback) {
  auto success_callback = base::Bind(
      [](const SmartAttributesCallback& callback, const std::string& result) {
        callback.Run(result, nullptr);
      },
      callback);
  auto error_callback = base::Bind(
      [](const SmartAttributesCallback& callback, brillo::Error* error) {
        callback.Run(std::string(), error);
      },
      callback);
  debugd_proxy_->SmartctlAsync(kSmartAttributesOption, success_callback,
                               error_callback);
  return;
}

}  // namespace diagnostics
