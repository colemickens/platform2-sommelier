#!/usr/bin/env python
#
# Copyright (C) 2009 The Android Open Source Project
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

"""Return information about routing table entries

Read and parse the system routing table. There are
two classes defined here: NetworkRoutes, which contains
information about all routes, and IPv4Route, which describes
a single routing table entry.
"""

ROUTES_V4_FILE = "/proc/net/route"
# The following constants are from <net/route.h>
RTF_UP      = 0x0001
RTF_GATEWAY = 0x0002
RTF_HOST    = 0x0004

import socket
import struct

class IPv4Route(object):
    def __init__(self, iface, dest, gway, flags, mask):
        self.interface = iface
        self.destination = int(dest, 16)
        self.gateway = int(gway, 16)
        self.flagbits = int(flags, 16)
        self.netmask = int(mask, 16)

    def _intToIp(self, addr):
        return socket.inet_ntoa(struct.pack('@I', addr))

    def _ipToInt(self, ip):
        return struct.unpack('I', socket.inet_aton(ip))[0]

    def __str__(self):
        flags = ""
        if self.flagbits & RTF_UP:
            flags += "U"
        if self.flagbits & RTF_GATEWAY:
            flags += "G"
        if self.flagbits & RTF_HOST:
            flags += "H"
        return "<%s dest: %s gway: %s mask: %s flags: %s>" % (
                self.interface,
                self._intToIp(self.destination),
                self._intToIp(self.gateway),
                self._intToIp(self.netmask),
                flags)

    def isUsable(self):
        return self.flagbits & RTF_UP

    def isHostRoute(self):
        return self.flagbits & RTF_HOST

    def isGatewayRoute(self):
        return self.flagbits & RTF_GATEWAY

    def isInterfaceRoute(self):
        return (self.flagbits & RTF_GATEWAY) == 0

    def isDefaultRoute(self):
        return (self.flagbits & RTF_GATEWAY) and self.destination == 0

    def matches(self, ip):
        return (self._ipToInt(ip) & self.netmask) == self.destination


def parseIPv4Routes(routelist):
    # The first line is headers that will allow us
    # to correctly interpret the values in the following
    # lines
    colMap = {}
    headers = routelist[0].split()
    for (pos, token) in enumerate(headers):
        colMap[token] = pos

    routes = []
    for routeline in routelist[1:]:
        route = routeline.split()
        interface = route[colMap["Iface"]]
        destination = route[colMap["Destination"]]
        gateway = route[colMap["Gateway"]]
        flags = route[colMap["Flags"]]
        mask = route[colMap["Mask"]]
        routes.append(IPv4Route(interface, destination, gateway, flags, mask))

    return routes


class NetworkRoutes(object):
    def __init__(self, routelist_v4=None):
        if routelist_v4 is None:
            with open(ROUTES_V4_FILE) as routef_v4:
                routelist_v4 = routef_v4.readlines()

        self.routes = parseIPv4Routes(routelist_v4)

    def _filterUsableRoutes(self):
        return (rr for rr in self.routes if rr.isUsable())

    def hasDefaultRoute(self, interface):
        return any(rr for rr in self._filterUsableRoutes()
                   if (rr.interface == interface and rr.isDefaultRoute()))

    def getDefaultRoutes(self):
        return [rr for rr in self._filterUsableRoutes() if rr.isDefaultRoute()]

    def hasInterfaceRoute(self, interface):
        return any(rr for rr in self._filterUsableRoutes()
                   if (rr.interface == interface and rr.isInterfaceRoute()))

    def getRouteFor(self, ip):
        for rr in self._filterUsableRoutes():
            if rr.matches(ip):
                return rr
        return None


if __name__ == "__main__":
    routes = NetworkRoutes()
    if len(routes.routes) == 0:
        print "Failed to read routing table"
    else:
        for each_route in routes.routes:
            print each_route

        print "hasDefaultRoute(\"eth0\"):", routes.hasDefaultRoute("eth0")

        dflts = routes.getDefaultRoutes()
        if len(dflts) == 0:
            print "There are no default routes"
        else:
            print "There are %d default routes" % len(dflts)


        print "hasInterfaceRoute(\"eth0\"):", routes.hasInterfaceRoute("eth0")

    routes = NetworkRoutes([
        "Iface Destination Gateway  Flags RefCnt "
        "Use Metric Mask MTU Window IRTT",
        "ones 00010203 FE010203 0007 0 0 0 00FFFFFF 0 0 0\n",
        "default 00000000 09080706 0007 0 0 0 00000000 0 0 0\n",
        ])

    print routes.getRouteFor("3.2.1.1")
    print routes.getRouteFor("9.2.1.8")
