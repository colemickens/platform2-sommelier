// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_LAUNCHER_POOLED_RESOURCE_H_
#define VM_LAUNCHER_POOLED_RESOURCE_H_

#include <string>

namespace vm_launcher {

// Manages a limited resource that needs to be assigned to each VM.
// Since multiple instances of vm_launcher can be run at once, this class
// assists in keeping track of allocated resources in a file. fcntl locking
// is used to guarantee that only one instance of vm_launcher may access the
// list of allocated resources at a time.
class PooledResource {
 public:
  virtual ~PooledResource() = default;

 protected:
  PooledResource() = default;

  // Allocates a resource from the pool. Returns true if the allocation
  // succeeded, or false otherwise.
  bool Allocate();

  // Releases a resource back into the pool. Returns true if the release
  // succeeded, or false otherwise.
  bool Release();

  // Returns a name for the resource, which will be used as the filename
  // for keeping track of that resource.
  virtual const char* GetName() const = 0;

  // Parses resources from the given string (originally the contents of the
  // associated resource file) so that an allocation can check for which
  // resources are available. Returns true if the load succeeded, or false
  // otherwise.
  virtual bool LoadResources(const std::string& resources) = 0;

  // Persists the in-memory allocated resources back to the resources file.
  // Returns a string representing the currently allocated resources, which
  // will be written back to the resources file.
  virtual std::string PersistResources() = 0;

  // Allocates a resource from the in-memory list of allocated resources.
  // A later call to PersistResources will save this to disk.
  // Returns true if the allocation succeeded, or false otherwise.
  virtual bool AllocateResource() = 0;

  // Removes a resource from the in-memory list of allocated resources.
  // A later call to PersistResources will save this to disk.
  // Returns true if the release succeeded, or false otherwise.
  virtual bool ReleaseResource() = 0;
};
}  // namespace vm_launcher

#endif  // VM_LAUNCHER_POOLED_RESOURCE_H_
