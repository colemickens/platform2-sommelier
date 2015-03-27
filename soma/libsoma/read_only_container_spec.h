// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIBSOMA_READ_ONLY_CONTAINER_SPEC_H_
#define SOMA_LIBSOMA_READ_ONLY_CONTAINER_SPEC_H_

#include <sched.h>
#include <sys/types.h>

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

// NB: library headers refer to each other with relative paths, so that includes
// make sense at both build and install time.
#include "soma_export.h"  // NOLINT(build/include)

namespace soma {

class ContainerSpec;

// More friendly read-only wrapper around the ContainerSpec protobuf.
// This class owns an instance of the protobuf and will hand out data from it
// on request.
class SOMA_EXPORT ReadOnlyContainerSpec {
 public:
  enum class Namespace {
    NEWIPC = CLONE_NEWIPC,
    NEWNET = CLONE_NEWNET,
    NEWNS = CLONE_NEWNS,
    NEWPID = CLONE_NEWPID,
    NEWUSER = CLONE_NEWUSER,
    NEWUTS = CLONE_NEWUTS,
    INVALID,
  };

  explicit ReadOnlyContainerSpec(const ContainerSpec* spec);
  virtual ~ReadOnlyContainerSpec();

  const base::FilePath& service_bundle_path() const {
    return service_bundle_path_;
  }
  uid_t uid() const;
  gid_t gid() const;

  const std::vector<std::string>& command_line() const {
    return command_line_;
  }

  const base::FilePath& working_directory() const { return working_dir_; }

  const std::vector<Namespace>& namespaces() const { return namespaces_; }

  bool all_tcp_ports_allowed() const;
  bool all_udp_ports_allowed() const;

  const std::vector<uint32_t>& tcp_listen_ports() const {
    return tcp_listen_ports_;
  }
  const std::vector<uint32_t>& udp_listen_ports() const {
    return udp_listen_ports_;
  }
  const std::vector<std::pair<int, int>>& device_node_filters() const {
    return device_node_filters_;
  }
  const std::vector<base::FilePath>& device_path_filters() const {
    return device_path_filters_;
  }

 private:
  const ContainerSpec* const internal_;

  // Caching for values in internal_, so this class can hand out const
  // references without having to vend handles to the underlying protobuf.
  const base::FilePath service_bundle_path_;
  const std::vector<std::string> command_line_;
  const base::FilePath working_dir_;
  const std::vector<uint32_t> tcp_listen_ports_;
  const std::vector<uint32_t> udp_listen_ports_;

  // Non-const because these need to be translated from the proto
  // representation to something more generic.
  std::vector<Namespace> namespaces_;
  std::vector<base::FilePath> device_path_filters_;
  std::vector<std::pair<int, int>> device_node_filters_;  // major, minor

  DISALLOW_COPY_AND_ASSIGN(ReadOnlyContainerSpec);
};

}  // namespace soma
#endif  // SOMA_LIBSOMA_READ_ONLY_CONTAINER_SPEC_H_
