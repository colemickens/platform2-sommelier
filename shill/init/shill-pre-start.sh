# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

bootstat shill-start

# Create state directory for shill.
mkdir -p /run/shill
chown -R shill:shill /run/shill
chmod 0755 /run/shill

# Create state directory for IPsec
mkdir -p /run/ipsec
chown -R shill:shill /run/ipsec
chmod 0770 /run/ipsec

# Create state directory for entire L2TP/IPsec subtree
mkdir -p /run/l2tpipsec_vpn
chown -R shill:shill /run/l2tpipsec_vpn
chmod 0750 /run/l2tpipsec_vpn

# Create storage for the shill global profile.
mkdir -p /var/cache/shill
chown -R shill:shill /var/cache/shill
chmod 0755 /var/cache/shill

# Set up dhcpcd's /var/{lib|run} dirs to run as user 'dhcp'.
mkdir -m 0755 -p /var/lib/dhcpcd
mkdir -m 0755 -p /run/dhcpcd
chmod -R u+rwX,g+rX,o+rX /var/lib/dhcpcd
chown -R dhcp:dhcp /var/lib/dhcpcd
chown -R dhcp:dhcp /run/dhcpcd

# Shill needs read access to this file, which is part of the in-kernel
# Connection Tracking System.
chown root:shill /proc/net/ip_conntrack
chmod g+r /proc/net/ip_conntrack

# Use flimflam's default profile if shill doesn't have one.
if [ ! -f /var/cache/shill/default.profile -a \
       -f /var/cache/flimflam/default.profile ]; then
  mv /var/cache/flimflam/default.profile /var/cache/shill/default.profile
  chmod a+r /var/cache/shill/default.profile
fi

# Create private directory for data which needs to persists across sessions.
mkdir -p /var/lib/shill
chown shill:shill /var/lib/shill
chmod 0755 /var/lib/shill

# Create directory for backing files for metrics.
mkdir -p /var/lib/shill/metrics
chown -R shill:shill /var/lib/shill/metrics
chmod 0755 /var/lib/shill/metrics

# This option is no longer supported.
rm -f /home/chronos/.disable_shill
