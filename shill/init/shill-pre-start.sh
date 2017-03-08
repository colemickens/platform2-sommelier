#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


bootstat shill-start

# Create state directory
mkdir -p /run/shill

# Create storage for the shill global profile.
mkdir -p /var/cache/shill

# Set up dhcpcd's /var/{lib|run} dirs to run as user 'dhcp'.
mkdir -m 0755 -p /var/lib/dhcpcd
mkdir -m 0755 -p /run/dhcpcd
chmod -R u+rwX,g+rX,o+rX /var/lib/dhcpcd
chown -R dhcp:dhcp /var/lib/dhcpcd
chown -R dhcp:dhcp /run/dhcpcd

# Use flimflam's default profile if shill doesn't have one.
if [ ! -f /var/cache/shill/default.profile -a \
       -f /var/cache/flimflam/default.profile ]; then
  mv /var/cache/flimflam/default.profile /var/cache/shill/default.profile
  chmod a+r /var/cache/shill/default.profile
fi

# This option is no longer supported.
rm -f /home/chronos/.disable_shill
