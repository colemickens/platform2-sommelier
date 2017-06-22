// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_CONSTANTS_H_
#define MIDIS_CONSTANTS_H_

namespace midis {

// A single variable of |struct MidisDeviceInfo| has both name and manufacturer
// strings. These alone will take up 512 bytes. Since kMaxBufSize is used to
// create the buffer which lists the connected devices, we set it to a
// sufficient value so that at least 7 devices can be represented.
// TODO(pmalani): Revisit this value based on how many simultaneous devices we
// expect to support.
const int kMaxBufSize = 4096;

}  // namespace midis

#endif  // MIDIS_CONSTANTS_H_
