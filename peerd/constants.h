// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_CONSTANTS_H_
#define PEERD_CONSTANTS_H_

namespace peerd {

namespace constants {

extern const char kSerbusServiceId[];

namespace mdns {

// The record type of a serbus record.
extern const char kSerbusServiceType[];
// Keys inside the Serbus TXT record.
extern const char kSerbusVersion[];
extern const char kSerbusPeerId[];
extern const char kSerbusName[];
extern const char kSerbusNote[];
extern const char kSerbusServiceList[];

extern const char kSerbusServiceDelimiter;

}  // namespace mdns

}  // namespace constants

}  // namespace peerd

#endif  // PEERD_CONSTANTS_H_
