# Portier

A Neighbor Discovery Proxy (ND Proxy) service for Chrome OS.  This is an
expanded implementation of a ND Proxy as defined in [RFC 4389].

## Neighbor Discovery Proxy Summary

ND Proxy is used on a network node to bridge two IPv6 network segments,
without any other part of the network being aware that there is a
bridge.  This is done through a combination of MAC address translation and
IP packet *proxying*.

The main difference between *proxying* and *forwarding* is that the node that
does the proxying does not advertise itself as a router and *most* proxied
IPv6 packets do not have any of their fields modified (such as the Hop Limit).
The only exception to not modifying IPv6 packets are certain ICMPv6 messages
which are used in the Neighbor Discovery protocol.

The Neighbor Discovery (ND) protocol is defined in [RFC 4861] and is critical
for IPv6 connectivity on containers and VMs living in Chrome OS.  However, the
specification of ND is designed in such a way that bridging two networks
cannot be implemented using the same methods used in IPv4.  Some of the
alternatives to bridging that are available in IPv4, such as NAT, are
available in IPv6 though provide limited value on an IPv6 network.

## About the Portier Service

The Portier service is intended to implement a subset of the ND Proxy
protocol, while also providing some additional networking features which
are needed by some other Chrome OS services (such as CrosVM and ARC++).

Due to limitations in the Linux kernel neighbor cache and the configuration
available to the Linux user-space for networking processes, not all features
of ND Proxy can be implemented.  The key features that are not supported
include some of the requirements of when to send ICMP error messages as the
kernal will always send certain ones, even if their logical meaning
is different under ND Proxy.  There are also some ND processes which are
not performed by Portier which are suggested by the ND Proxy specification.

The main additional feature between Portier and a ND Proxy specification
is that *proxy interfaces* are grouped together to provide limited network
scope.  The specific usecase in Chrome OS is to allow VMs with multiple
network interfaces to be individually bridged to physical network interfaces
on the host.

## Using Portier

Portier is currently in development.  More information about its design and
how to use it will be made available when ready.


[RFC 4389]: https://tools.ietf.org/html/rfc4389
[RFC 4861]: https://tools.ietf.org/html/rfc4861
