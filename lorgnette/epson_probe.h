// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LORGNETTE_EPSON_PROBE_H_
#define LORGNETTE_EPSON_PROBE_H_

#include "lorgnette/manager.h"

// Method for probing for Epson-based network scanners.  The code in
// sane-backends for probing Epson based scanners does not work in
// Chrome OS since it expects a unicast reply to an outgoing broadcast
// probe.  This protocol is simple enough to implement within lorgnette
// and can take advantage of the FirewallManager to temporarily open up
// access to receive a reply.
namespace lorgnette {

class FirewallManager;

namespace epson_probe {

// Probe for Epson-based network scanners.  Use |firewall_manager| to request
// firewall permissions for receiving probe replies.  Return any new entries
// into |scanner_list|.
void ProbeForScanners(FirewallManager* firewall_manager,
                      Manager::ScannerInfo* scanner_list);

}  // namespace epson_probe

}  // namespace lorgnette

#endif  // LORGNETTE_EPSON_PROBE_H_
