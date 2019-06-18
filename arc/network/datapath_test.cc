// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/datapath.h"

#include <utility>
#include <vector>

#include <base/strings/string_util.h>

#include <gtest/gtest.h>

#include "arc/network/fake_process_runner.h"

namespace arc_networkd {

TEST(DatapathTest, AddLegacyIPv4DNAT) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.AddLegacyIPv4DNAT("1.2.3.4");
  runner.VerifyRuns(
      {"/sbin/iptables -t nat -N dnat_arc -w",
       "/sbin/iptables -t nat -A dnat_arc -j DNAT --to-destination "
       "1.2.3.4 -w",
       "/sbin/iptables -t nat -N try_arc -w",
       "/sbin/iptables -t nat -A PREROUTING -m socket --nowildcard -j ACCEPT "
       "-w",
       "/sbin/iptables -t nat -A PREROUTING -p tcp -j try_arc -w",
       "/sbin/iptables -t nat -A PREROUTING -p udp -j try_arc -w"});
}

TEST(DatapathTest, RemoveLegacyIPv4DNAT) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveLegacyIPv4DNAT();
  runner.VerifyRuns(
      {"/sbin/iptables -t nat -D PREROUTING -p udp -j try_arc -w",
       "/sbin/iptables -t nat -D PREROUTING -p tcp -j try_arc -w",
       "/sbin/iptables -t nat -D PREROUTING -m socket --nowildcard -j "
       "ACCEPT -w",
       "/sbin/iptables -t nat -F try_arc -w",
       "/sbin/iptables -t nat -X try_arc -w",
       "/sbin/iptables -t nat -F dnat_arc -w",
       "/sbin/iptables -t nat -X dnat_arc -w"});
}

TEST(DatapathTest, AddInboundIPv4DNAT) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.AddInboundIPv4DNAT("eth0", "1.2.3.4");
  runner.VerifyRuns(
      {"/sbin/iptables -t nat -A PREROUTING -i eth0 -m socket --nowildcard -j "
       "ACCEPT "
       "-w",
       "/sbin/iptables -t nat -A PREROUTING -i eth0 -p tcp -j DNAT "
       "--to-destination 1.2.3.4 -w",
       "/sbin/iptables -t nat -A PREROUTING -i eth0 -p udp -j DNAT "
       "--to-destination 1.2.3.4 -w"});
}

TEST(DatapathTest, RemoveInboundIPv4DNAT) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveInboundIPv4DNAT("eth0", "1.2.3.4");
  runner.VerifyRuns(
      {"/sbin/iptables -t nat -D PREROUTING -i eth0 -p udp -j DNAT "
       "--to-destination 1.2.3.4 -w",
       "/sbin/iptables -t nat -D PREROUTING -i eth0 -p tcp -j DNAT "
       "--to-destination 1.2.3.4 -w",
       "/sbin/iptables -t nat -D PREROUTING -i eth0 -m socket --nowildcard "
       "-j ACCEPT -w"});
}

TEST(DatapathTest, AddOutboundIPv4) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.AddOutboundIPv4("eth0");
  runner.VerifyRuns(
      {"/sbin/iptables -t filter -A FORWARD -o eth0 -j ACCEPT -w"});
}

TEST(DatapathTest, RemoveInboundIPv4) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveOutboundIPv4("eth0");
  runner.VerifyRuns(
      {"/sbin/iptables -t filter -D FORWARD -o eth0 -j ACCEPT -w"});
}

}  // namespace arc_networkd
