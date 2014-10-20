// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/constants.h"

namespace peerd {

namespace constants {

const char kSerbusServiceId[] = "serbus";

namespace mdns {

const char kSerbusServiceType[] = "_serbus._tcp";

const char kSerbusVersion[] = "ver";
const char kSerbusPeerId[] = "id";
const char kSerbusName[] = "name";
const char kSerbusNote[] = "note";
const char kSerbusServiceList[] = "services";

const char kSerbusServiceDelimiter = '.';

}  // namespace mdns

}  // namespace constants

}  // namespace peerd
