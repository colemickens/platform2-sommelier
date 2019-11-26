// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/datapath.h"

#include <linux/if_tun.h>
#include <sys/ioctl.h>

#include <set>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/strings/string_util.h>
#include <gtest/gtest.h>

#include "arc/network/fake_process_runner.h"
#include "arc/network/net_util.h"

namespace arc_networkd {
namespace {

std::set<unsigned long> ioctl_reqs;

// Capture all ioctls and succeed.
int ioctl_req_cap(int fd, unsigned long req, ...) {
  ioctl_reqs.insert(req);
  return 0;
}

}  // namespace

TEST(DatapathTest, AddTAP) {
  FakeProcessRunner runner;
  Datapath datapath(&runner, ioctl_req_cap);
  MacAddress mac = {1, 2, 3, 4, 5, 6};
  Subnet subnet(Ipv4Addr(100, 115, 92, 4), 30, base::Bind(&base::DoNothing));
  auto addr = subnet.AllocateAtOffset(0);
  auto ifname = datapath.AddTAP("foo0", &mac, addr.get(), "");
  EXPECT_EQ(ifname, "foo0");
  std::set<unsigned long> expected = {
      TUNSETIFF,     TUNSETPERSIST, SIOCSIFADDR, SIOCSIFNETMASK,
      SIOCSIFHWADDR, SIOCGIFFLAGS,  SIOCSIFFLAGS};
  EXPECT_EQ(ioctl_reqs, expected);
  ioctl_reqs.clear();
}

TEST(DatapathTest, AddTAPWithOwner) {
  FakeProcessRunner runner;
  Datapath datapath(&runner, ioctl_req_cap);
  MacAddress mac = {1, 2, 3, 4, 5, 6};
  Subnet subnet(Ipv4Addr(100, 115, 92, 4), 30, base::Bind(&base::DoNothing));
  auto addr = subnet.AllocateAtOffset(0);
  auto ifname = datapath.AddTAP("foo0", &mac, addr.get(), "root");
  EXPECT_EQ(ifname, "foo0");
  std::set<unsigned long> expected = {
      TUNSETIFF,      TUNSETPERSIST, TUNSETOWNER,  SIOCSIFADDR,
      SIOCSIFNETMASK, SIOCSIFHWADDR, SIOCGIFFLAGS, SIOCSIFFLAGS};
  EXPECT_EQ(ioctl_reqs, expected);
  ioctl_reqs.clear();
}

TEST(DatapathTest, AddTAPNoAddrs) {
  FakeProcessRunner runner;
  Datapath datapath(&runner, ioctl_req_cap);
  auto ifname = datapath.AddTAP("foo0", nullptr, nullptr, "");
  EXPECT_EQ(ifname, "foo0");
  std::set<unsigned long> expected = {TUNSETIFF, TUNSETPERSIST, SIOCGIFFLAGS,
                                      SIOCSIFFLAGS};
  EXPECT_EQ(ioctl_reqs, expected);
  ioctl_reqs.clear();
}

TEST(DatapathTest, RemoveTAP) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveTAP("foo0");
  runner.VerifyRuns({"/bin/ip tuntap del foo0 mode tap"});
}

TEST(DatapathTest, AddBridge) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.AddBridge("br", "1.2.3.4");
  runner.VerifyRuns(
      {"/sbin/brctl addbr br",
       "/bin/ifconfig br 1.2.3.4 netmask 255.255.255.252 up",
       "/sbin/iptables -t mangle -A PREROUTING -i br -j MARK --set-mark 1 -w"});
}

TEST(DatapathTest, AddVirtualBridgedInterface) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  std::string peer =
      datapath.AddVirtualBridgedInterface("foo", "00:11:22", "brfoo");
  EXPECT_EQ(peer, "peer_foo");
  runner.VerifyRuns(
      {"/bin/ip link delete veth_foo",  // RemoveInterface is run first
       "/bin/ip link add veth_foo type veth peer name peer_foo",
       "/bin/ifconfig veth_foo up",
       "/bin/ip link set dev peer_foo addr 00:11:22 down",
       "/sbin/brctl addif brfoo veth_foo"});
}

TEST(DatapathTest, RemoveInterface) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveInterface("foo");
  runner.VerifyRuns({"/bin/ip link delete foo"});
}

TEST(DatapathTest, RemoveBridge) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveBridge("br");
  runner.VerifyRuns(
      {"/sbin/iptables -t mangle -D PREROUTING -i br -j MARK --set-mark 1 -w",
       "/bin/ifconfig br down", "/sbin/brctl delbr br"});
}

TEST(DatapathTest, AddInterfaceToContainer) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.AddInterfaceToContainer(123, "src", "dst", "1.2.3.4", true);
  runner.VerifyRuns({"/bin/ip link set src netns 123"});
  runner.VerifyAddInterface("src", "dst", "1.2.3.4", "255.255.255.252", true,
                            "123");
}

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

TEST(DatapathTest, AddLegacyIPv4InboundDNAT) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.AddLegacyIPv4InboundDNAT("wlan0");
  runner.VerifyRuns(
      {"/sbin/iptables -t nat -A try_arc -i wlan0 -j dnat_arc -w"});
}

TEST(DatapathTest, RemoveLegacyIPv4InboundDNAT) {
  FakeProcessRunner runner;
  runner.Capture(true);
  Datapath datapath(&runner);
  datapath.RemoveLegacyIPv4InboundDNAT();
  runner.VerifyRuns({"/sbin/iptables -t nat -F try_arc -w"});
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
