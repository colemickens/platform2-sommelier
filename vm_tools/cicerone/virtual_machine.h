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

#include "container_guest.grpc.pb.h"  // NOLINT(build/include)
#include "tremplin.grpc.pb.h"         // NOLINT(build/include)

namespace vm_tools {
namespace cicerone {

// Represents a single instance of a virtual machine.
class VirtualMachine {
 public:
  // Linux application ID and its icon content.
  struct Icon {
    std::string desktop_file_id;
    std::string content;
  };
  // Information about a Linux package file.
  struct LinuxPackageInfo {
    std::string package_id;
    std::string license;
    std::string description;
    std::string project_url;
    uint64_t size;
    std::string summary;
  };

  enum class CreateLxdContainerStatus {
    UNKNOWN,
    CREATING,
    EXISTS,
    FAILED,
  };

  enum class StartLxdContainerStatus {
    UNKNOWN,
    STARTED,
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

  VirtualMachine(uint32_t container_subnet,
                 uint32_t container_netmask,
                 uint32_t ipv4_address);
  ~VirtualMachine();

  // The VM's container subnet netmask in network byte order.
  uint32_t container_netmask() const { return container_netmask_; }

  // The first address in the VM's container subnet in network byte order.
  uint32_t container_subnet() const { return container_subnet_; }

  // The first address in the VM's container subnet in network byte order.
  uint32_t ipv4_address() const { return ipv4_address_; }

  // Connect to the tremplin instance in the VM.
  bool ConnectTremplin(const std::string& uri);

  // Registers a container with the VM using the |container_ip| address and
  // |container_token|. Returns true if the token is valid, false otherwise.
  bool RegisterContainer(const std::string& container_token,
                         const std::string& container_ip);

  // Unregister a container with |container_token| within this VM. Returns true
  // if the token is valid, false otherwise.
  bool UnregisterContainer(const std::string& container_token);

  // Generates a random token string that should be passed into the container
  // which can then be used by the container to identify itself when it
  // communicates back with us.
  std::string GenerateContainerToken(const std::string& container_name);

  // Returns the name of the container associated with the passed in
  // |container_token|. Returns the empty string if no such mapping exists. This
  // will only return a name that has been confirmed after calling
  // RegisterContainerIp.
  std::string GetContainerNameForToken(const std::string& container_token);

  // Launches the application associated with |desktop_file_id| in the container
  // named |container_name| within this VM. Returns true on success, false
  // otherwise and fills out |out_error| on failure.
  bool LaunchContainerApplication(const std::string& container_name,
                                  const std::string& desktop_file_id,
                                  std::vector<std::string> files,
                                  std::string* out_error);

  // Launches vshd.
  bool LaunchVshd(const std::string& container_name,
                  uint32_t port,
                  std::string* out_error);

  // Gets information about a Linux package from container |container_name| from
  // the container's filesystem at |file_path|. Returns true on success, false
  // otherwise. On failure, |out_error| is set with failure details. On success,
  // the fields in the |out_pkg_info| struct will be filled in.
  bool GetLinuxPackageInfo(const std::string& container_name,
                           const std::string& file_path,
                           LinuxPackageInfo* out_pkg_info,
                           std::string* out_error);

  // Installs a Linux package into container |container_name| from the
  // container's filesystem at |file_path|. Returns a status value which
  // corresponds to the Status enum in the InstallLinuxPackageResponse protobuf
  // (either the cicerone or container_guest one, they have matching values),
  // if the status is FAILED then |out_error| is set with failure details.
  int InstallLinuxPackage(const std::string& container_name,
                          const std::string& file_path,
                          std::string* out_error);

  // Gets container debug information.
  bool GetDebugInformation(const std::string& container_name, std::string* out);

  // Returns whether there is a connected stub to Garcon running inside the
  // named |container_name| within this VM.
  bool IsContainerRunning(const std::string& container_name);

  // Gets icons of those applications with their desktop file IDs specified
  // by |desktop_file_ids| from the container named |container_name| within
  // this VM. The icons should have size of |icon_size| and designed scale of
  // |scale|. The icons are returned through the paramenter |icons|.
  bool GetContainerAppIcon(const std::string& container_name,
                           std::vector<std::string> desktop_file_ids,
                           uint32_t icon_size,
                           uint32_t scale,
                           std::vector<Icon>* icons);

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
      std::string* out_error);

  // Gets the primary user of an LXD container.
  GetLxdContainerUsernameStatus GetLxdContainerUsername(
      const std::string& container_name,
      std::string* username,
      std::string* out_error);

  // Sets up an LXD container.
  SetUpLxdContainerUserStatus SetUpLxdContainerUser(
      const std::string& container_name,
      const std::string& container_username,
      std::string* out_error);

  // Gets a list of all the active container names in this VM.
  std::vector<std::string> GetContainerNames();

 private:
  uint32_t container_subnet_;
  uint32_t container_netmask_;
  uint32_t ipv4_address_;

  // Mapping of container tokens to names. The tokens are used to securely
  // identify a container when it connects back to concierge to identify itself.
  std::map<std::string, std::string> container_token_to_name_;

  // Pending map of container tokens to names. The tokens are put in here when
  // they are generated and removed once we have a connection from the
  // container. We do not immediately put them in the contaienr_token_to_name_
  // map because we may get redundant requests to start a container that is
  // already running and we don't want to invalidate an in-use token.
  std::map<std::string, std::string> pending_container_token_to_name_;

  // The stub for the tremplin instance in this VM.
  std::unique_ptr<vm_tools::tremplin::Tremplin::Stub> tremplin_stub_;

  // Mapping of container names to a stub for making RPC requests to the garcon
  // process inside the container.
  std::map<std::string, std::unique_ptr<vm_tools::container::Garcon::Stub>>
      container_name_to_garcon_stub_;

  // Mapping of container names to a grpc Channel to the garcon process inside
  // the container, which we can test for connectedness.
  std::map<std::string, std::shared_ptr<grpc::Channel>>
      container_name_to_garcon_channel_;

  DISALLOW_COPY_AND_ASSIGN(VirtualMachine);
};

}  // namespace cicerone
}  // namespace vm_tools

#endif  // VM_TOOLS_CICERONE_VIRTUAL_MACHINE_H_
