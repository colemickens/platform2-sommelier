// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_TECHNOLOGIES_H_
#define PEERD_TECHNOLOGIES_H_

#include <stdint.h>

namespace peerd {
namespace technologies {

using tech_t = uint32_t;

extern const tech_t kAll;
extern const tech_t kMDNS;

}  // namespace technologies
}  // namespace peerd

#endif  // PEERD_TECHNOLOGIES_H_
