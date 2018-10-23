// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DPSL_INTERNAL_DPSL_GLOBAL_CONTEXT_IMPL_H_
#define DIAGNOSTICS_DPSL_INTERNAL_DPSL_GLOBAL_CONTEXT_IMPL_H_

#include <base/macros.h>
#include <base/threading/thread_checker_impl.h>

#include "diagnostics/dpsl/public/dpsl_global_context.h"

namespace diagnostics {

// Real implementation of the DpslGlobalContext interface.
//
// Currently only does the logging configuration on construction.
class DpslGlobalContextImpl final : public DpslGlobalContext {
 public:
  DpslGlobalContextImpl();
  ~DpslGlobalContextImpl() override;

 private:
  base::ThreadCheckerImpl thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DpslGlobalContextImpl);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DPSL_INTERNAL_DPSL_GLOBAL_CONTEXT_IMPL_H_
