# `vm_launcher`

## Overview
`vm_launcher` is a frontend to kvmtool and crosvm that is meant to simplify the
interface for launching a VM. This includes taking care of housekeeping tasks
that don't have an appropriate home elsewhere, such as managing available
IPv4 subnets and MAC addresses.

`vm_launcher` can also set up NFS server, and the VM will mount the NFS 
automatically. NFS server is started via `vm_launcher` because for each
VM we need to modify NFS server configurations and restart it.