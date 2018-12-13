// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CICERONE_VIRTUAL_MACHINE_H_
#define VM_TOOLS_CICERONE_VIRTUAL_MACHINE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "vm_tools/cicerone/container.h"

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)
#include "tremplin.grpc.pb.h"         // NOLINT(build/include)

namespace vm_tools {
namespace cicerone {

// Represents a single instance of a virtual machine.
class VirtualMachine {
 public:
  enum class CreateLxdContainerStatus {
    UNKNOWN,
    CREATING,
    EXISTS,
    FAILED,
  };

  enum class StartLxdContainerStatus {
    UNKNOWN,
    STARTING,
    STARTED,
    REMAPPING,
    RUNNING,
    FAILED,
  };

  enum class GetLxdContainerUsernameStatus {
    UNKNOWN,
    SUCCESS,
    CONTAINER_NOT_FOUND,
    CONTAINER_NOT_RUNNING,
    USER_NOT_FOUND,
    FAILED,
  };

  enum class SetUpLxdContainerUserStatus {
    UNKNOWN,
    SUCCESS,
    EXISTS,
    FAILED,
  };

  enum class GetLxdContainerInfoStatus {
    UNKNOWN,
    RUNNING,
    STOPPED,
    NOT_FOUND,
    FAILED,
  };

  // Info about the LXD container.
  struct LxdContainerInfo {
    // The IPv4 address of the container in network byte order.
    // This field is only valid if the container status is RUNNING.
    uint32_t ipv4_address;
  };

  // Results of a set timezone request
  struct SetTimezoneResults {
    int successes;
    std::vector<std::string> failure_reasons;
  };

  VirtualMachine(uint32_t container_subnet,
                 uint32_t container_netmask,
                 uint32_t ipv4_address,
                 uint32_t cid);
  ~VirtualMachine();

  // The VM's container subnet netmask in network byte order.
  uint32_t container_netmask() const { return container_netmask_; }

  // The first address in the VM's container subnet in network byte order.
  uint32_t container_subnet() const { return container_subnet_; }

  // The first address in the VM's container subnet in network byte order.
  uint32_t ipv4_address() const { return ipv4_address_; }

  // The VM's cid.
  uint32_t cid() const { return vsock_cid_; }

  // Call during unit tests to force this class to connect to the Tremplin
  // server at |tremplin_address| instead of the normal address. Must be called
  // before ConnectTremplin().
  void OverrideTremplinAddressForTesting(const std::string& tremplin_address);

  // Connect to the tremplin instance in the VM.
  bool ConnectTremplin();

  // Tries to set the default timezone for all containers in this VM to
  // |timezone_name|. If that fails, falls back to setting the TZ environment
  // variable to |posix_tz_string|.
  //
  // If setting the timezone fails entirely due to high-level issues
  // (e.g. tremplin not connected, rpc failed), this will return false and set
  // |out_error|.
  //
  // Otherwise, the results from individual containers will be stored in
  // |out_results|.
  bool SetTimezone(const std::string& timezone_name,
                   const std::string& posix_tz_string,
                   SetTimezoneResults* out_results,
                   std::string* out_error);

  // Registers a container with the VM using the |container_ip| address,
  // |vsock_garcon_port|, and |container_token|. Returns true if the token is
  // valid, false otherwise.
  bool RegisterContainer(const std::string& container_token,
                         const uint32_t vsock_garcon_port,
                         const std::string& container_ip);

  // Unregister a container with |container_token| within this VM. Returns true
  // if the token is valid, false otherwise.
  bool UnregisterContainer(const std::string& container_token);

  // Generates a random token string that should be passed into the container
  // which can then be used by the container to identify itself when it
  // communicates back with us.
  std::string GenerateContainerToken(const std::string& container_name);

  // For testing only. Add a container with the indicated security token. This
  // is the only way to get a consistent security token for unit tests & fuzz
  // tests.
  void CreateContainerWithTokenForTesting(const std::string& container_name,
                                          const std::string& container_token);

  // Returns the name of the container associated with the passed in
  // |container_token|. Returns the empty string if no such mapping exists. This
  // will only return a name that has been confirmed after calling
  // RegisterContainer.
  std::string GetContainerNameForToken(const std::string& container_token);

  // Returns a pointer to the container associated with the passed in
  // |container_token|. Returns nullptr if the container does not exist.
  // This function will only return a container that has been confirmed after
  // calling RegisterContainer.
  //
  // The pointer returned is owned by VirtualMachine and may not be stored.
  Container* GetContainerForToken(const std::string& container_token);

  // Returns a pointer to the pending container associated with the passed in
  // |container_token|. Returns nullptr if the container does not exist.
  // This function will only return a container that has NOT been confirmed by
  // calling RegisterContainer.
  //
  // The pointer returned is owned by VirtualMachine and may not be stored.
  Container* GetPendingContainerForToken(const std::string& container_token);

  // Returns a pointer to the container associated with the passed in
  // |container_name|. Returns nullptr if the container does not exist.
  // This function will only return a name that has been confirmed after calling
  // RegisterContainer.
  //
  // The pointer returned is owned by VirtualMachine and may not be stored.
  Container* GetContainerForName(const std::string& container_name);

  // Creates an LXD container.
  CreateLxdContainerStatus CreateLxdContainer(const std::string& container_name,
                                              const std::string& image_server,
                                              const std::string& image_alias,
                                              std::string* out_error);

  // Starts an LXD container.
  StartLxdContainerStatus StartLxdContainer(
      const std::string& container_name,
      const std::string& container_private_key,
      const std::string& host_public_key,
      const std::string& token,
      bool async,
      std::string* out_error);

  // Gets the primary user of an LXD container.
  GetLxdContainerUsernameStatus GetLxdContainerUsername(
      const std::string& container_name,
      std::string* username,
      std::string* homedir,
      std::string* out_error);

  // Sets up an LXD container.
  SetUpLxdContainerUserStatus SetUpLxdContainerUser(
      const std::string& container_name,
      const std::string& container_username,
      std::string* out_username,
      std::string* out_error);

  // Gets info about an LXD container.
  GetLxdContainerInfoStatus GetLxdContainerInfo(
      const std::string& container_name,
      LxdContainerInfo* out_info,
      std::string* out_error);

  // Gets a list of all the active container names in this VM.
  std::vector<std::string> GetContainerNames();

 private:
  uint32_t container_subnet_;
  uint32_t container_netmask_;
  uint32_t ipv4_address_;

  // Virtual socket context id to be used when communicating with this VM.
  uint32_t vsock_cid_;

  // If set, our |tremplin_stub_| will always attempt to connect to this address
  // instead of the normal vsock address. For testing only.
  std::string tremplin_testing_address_;

  // Mapping of tokens to containers. The tokens are used to securely
  // identify a container when it connects back to concierge to identify itself.
  std::map<std::string, std::unique_ptr<Container>> containers_;

  // Pending map of tokens to containers. The tokens are put in here when
  // they are generated and removed once we have a connection from the
  // container. We do not immediately put them in the containers map because we
  // may get redundant requests to start a container that is already running
  // and we don't want to invalidate an in-use token.
  std::map<std::string, std::unique_ptr<Container>> pending_containers_;

  // The stub for the tremplin instance in this VM.
  std::unique_ptr<vm_tools::tremplin::Tremplin::Stub> tremplin_stub_;

  base::WeakPtrFactory<VirtualMachine> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VirtualMachine);
};

}  // namespace cicerone
}  // namespace vm_tools

#endif  // VM_TOOLS_CICERONE_VIRTUAL_MACHINE_H_
