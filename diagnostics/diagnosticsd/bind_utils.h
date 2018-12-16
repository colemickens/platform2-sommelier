// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_DIAGNOSTICSD_BIND_UTILS_H_
#define DIAGNOSTICS_DIAGNOSTICSD_BIND_UTILS_H_

#include <base/bind.h>
#include <base/callback.h>
#include <base/logging.h>

namespace diagnostics {

// Analog of base::BarrierClosure. TODO(emaxx, b/37434548): Remove this in
// favor of base::BarrierClosure once libchrome gets uprev'ed.
inline base::Closure BarrierClosure(int num_closures,
                                    const base::Closure& done_closure) {
  DCHECK_GT(num_closures, 0);
  return base::Bind(
      [](int* num_callbacks_left, base::Closure done_closure) {
        DCHECK_GT(*num_callbacks_left, 0);
        if (--*num_callbacks_left == 0)
          done_closure.Run();
      },
      base::Owned(new int(num_closures)), done_closure);
}

}  // namespace diagnostics

#endif  // DIAGNOSTICS_DIAGNOSTICSD_BIND_UTILS_H_
