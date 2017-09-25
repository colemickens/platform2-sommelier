// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_LAUNCHER_POOLED_RESOURCE_H_
#define VM_TOOLS_LAUNCHER_POOLED_RESOURCE_H_

#include <string>

#include <base/files/file_path.h>

namespace vm_tools {
namespace launcher {

// Manages a limited resource that needs to be assigned to each VM.
// Since multiple instances of launcher can be run at once, this class
// assists in keeping track of allocated resources in a file. fcntl locking
// is used to guarantee that only one instance of launcher may access the
// list of allocated resources at a time.
class PooledResource {
 public:
  virtual ~PooledResource() = default;

  // Set the |release_on_destruction| flag. This will cause the resource to be
  // released from the global pool when the object is destructed.
  void SetReleaseOnDestruction(bool release_on_destruction);

 protected:
  PooledResource(const base::FilePath& instance_runtime_dir,
                 bool release_on_destruction);

  // Allocates a resource from the pool. Returns true if the allocation
  // succeeded, or false otherwise.
  bool Allocate();

  // Releases a resource back into the pool. Returns true if the release
  // succeeded, or false otherwise.
  bool Release();

  // Load resources for an already running VM. Returns true if the runtime
  // data is read successfully, and false otherwise.
  bool LoadInstance();

  // Returns a name for the resource, which will be used as the filename
  // for keeping track of that resource.
  virtual const char* GetName() const = 0;

  // Returns a string identifier for the resource allocated. For example, for a
  // MAC address this could be the EUI-48 representation 01:23:45:67:89:0A.
  virtual const std::string GetResourceID() const = 0;

  // Parses resources from the given string (originally the contents of the
  // associated resource file) so that an allocation can check for which
  // resources are available. Returns true if the load succeeded, or false
  // otherwise.
  virtual bool LoadGlobalResources(const std::string& resources) = 0;

  // Persists the in-memory allocated resources back to the resources file.
  // Returns a string representing the currently allocated resources, which
  // will be written back to the resources file.
  virtual std::string PersistGlobalResources() = 0;

  // Load the resource allocated to the VM instance in this runtime directory.
  virtual bool LoadInstanceResource(const std::string& resource) = 0;

  // Persist the resource that was allocated to the VM instance's runtime
  // directory.
  bool PersistInstanceResource();

  // Allocates a resource from the in-memory list of allocated resources.
  // A later call to PersistResources will save this to disk.
  // Returns true if the allocation succeeded, or false otherwise.
  virtual bool AllocateResource() = 0;

  // Removes a resource from the in-memory list of allocated resources.
  // A later call to PersistResources will save this to disk.
  // Returns true if the release succeeded, or false otherwise.
  virtual bool ReleaseResource() = 0;

  // Returns true if a subclass should call Release() in its destructor.
  bool ShouldReleaseOnDestruction();

 private:
  // The VM instance's runtime directory.
  const base::FilePath instance_runtime_dir_;

  // Should resources be released when this object's destructor is called?
  bool release_on_destruction_;
};

}  // namespace launcher
}  // namespace vm_tools

#endif  // VM_TOOLS_LAUNCHER_POOLED_RESOURCE_H_
