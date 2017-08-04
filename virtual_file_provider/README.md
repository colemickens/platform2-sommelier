# Virtual File Provider

Virtual File Provider is a service which provides file descriptors which forward
access requests to chrome.
From the accessing process's perspective, the file descriptor behaves like a
regular file descriptor (unlike pipe, it's seekable), while actually there is no
real file associated with it.

## Private FUSE file system
To forward access requests on file descriptors, this service implements a FUSE
file system which is only accessible to this service itself.

## D-Bus interface
This service provides only one D-Bus method, OpenFile().
When OpenFile() is called, it generates a new unique ID, opens a file descriptor
on the private FUSE file system, and returns the ID and the FD to the caller.
The caller should remember the ID to handle access requests later.
When the FD is being accessed, this service sends signal to forward the access
request to chrome.
