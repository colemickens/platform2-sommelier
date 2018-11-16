// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_KEYMASTER_KEYMASTER_SERVER_H_
#define ARC_KEYMASTER_KEYMASTER_SERVER_H_

#include "mojo/keymaster.mojom.h"

namespace arc {
namespace keymaster {

class KeymasterServer : public arc::mojom::KeymasterServer {};

}  // namespace keymaster
}  // namespace arc

#endif  // ARC_KEYMASTER_KEYMASTER_SERVER_H_
