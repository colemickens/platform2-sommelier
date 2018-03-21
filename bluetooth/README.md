# Chrome OS Bluetooth Service

btdispatch is a daemon that enables replacing bluez with newblue, Chrome OS's
new Bluetooth stack. Chrome interacts with this daemon instead of bluez, and the
daemon splits Chrome's Bluetooth requests to be forwarded to either bluez (for
Bluetooth Classic) or newblue (for Bluetooth Low Energy). The complete design
doc is at go/bluez-stack-split.
