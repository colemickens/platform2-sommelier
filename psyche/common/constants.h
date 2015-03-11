// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_COMMON_CONSTANTS_H_
#define PSYCHE_COMMON_CONSTANTS_H_

namespace psyche {

// Name that psyched uses when registering itself with servicemanager and that
// libpsyche uses to request connections to psyched from servicemanager.
extern const char kPsychedServiceManagerName[];

}  // namespace psyche

#endif  // PSYCHE_COMMON_CONSTANTS_H_
