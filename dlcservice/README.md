# Downloadable Content (DLC) Service Daemon

Read [Developer Doc] instead on how to use DLC framework.

## dlcservice
This D-Bus daemon manages life-cycles of DLC Modules and provides APIs to
install/uninstall DLC modules.

## dlcservice-client
This is a generated D-Bus client library for dlcservice. Other system services
that intend to interact with dlcservice is supposed to use this library.

[Developer Doc]: docs/developer.md
