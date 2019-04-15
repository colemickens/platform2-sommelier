// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_CORE_DELEGATE_IMPL_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_CORE_DELEGATE_IMPL_H_

#include <memory>

#include <base/macros.h>
#include <brillo/daemons/daemon.h>

#include "diagnostics/wilco_dtc_supportd/wilco_dtc_supportd_core.h"

namespace diagnostics {

// Production implementation of WilcoDtcSupportdCore's delegate.
class WilcoDtcSupportdCoreDelegateImpl final
    : public WilcoDtcSupportdCore::Delegate {
 public:
  explicit WilcoDtcSupportdCoreDelegateImpl(brillo::Daemon* daemon);
  ~WilcoDtcSupportdCoreDelegateImpl() override;

  // WilcoDtcSupportdCore::Delegate overrides:
  std::unique_ptr<mojo::Binding<MojomWilcoDtcSupportdServiceFactory>>
  BindWilcoDtcSupportdMojoServiceFactory(
      MojomWilcoDtcSupportdServiceFactory* mojo_service_factory,
      base::ScopedFD mojo_pipe_fd) override;
  void BeginDaemonShutdown() override;

 private:
  // Unowned. The daemon must outlive this instance.
  brillo::Daemon* const daemon_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdCoreDelegateImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_CORE_DELEGATE_IMPL_H_
