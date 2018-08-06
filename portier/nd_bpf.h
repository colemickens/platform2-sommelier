// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_ND_BPF_H_
#define PORTIER_ND_BPF_H_

#include <linux/filter.h>  // BPF filter.

// Defines BPF filters that are needed for sparating IPv6 packets which
// are to be proxied.
namespace portier {

// A classic BPF filter which only allows ICMPv6 ND packets through.
// The specific ND messages allowed through are: RA, NS, NA and Redirect.
extern const struct sock_fprog kNeighborDiscoveryFilter;

// A classic BPF filter which allow all other IPv6 packets which are not
// allowed through by |kNeighborDiscoveryFilter|.
extern const struct sock_fprog kNonNeighborDiscoveryFilter;

}  // namespace portier

#endif  // PORTIER_ND_BPF_H_
