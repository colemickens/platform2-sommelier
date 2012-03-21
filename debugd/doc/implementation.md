# Implementation: debugd

[dbus-cxx]: http://sourceforge.net/projects/dbus-cplusplus/

The debug daemon uses [dbus-c++][dbus-cxx] to serve DBus clients. The daemon is
broken up into _tools_, which implement closely-related sets of functions that
can share significant code.

A tool is a set of closely related API functions that diagnoses one thing (for
example, ICMP connectivity). The debug daemon invokes the corresponding tool for
each of its API functions. Dividing the DBus interface from the implementations
of each tool keeps the DebugDaemon class from growing too large and makes it
easier to test the individual tools.

## cpplint exemptions:
* dbus-c++ passes the DBus::Error for a method handler to fill in as a non-const
  reference, and accepts arguments to many of its functions as non-const
  references.
