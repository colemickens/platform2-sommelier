// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_READ_ONLY_SANDBOX_SPEC_H_
#define SOMA_LIB_SOMA_READ_ONLY_SANDBOX_SPEC_H_

#include <sched.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/time/time.h>
#include <base/macros.h>
#include <soma/soma_export.h>

namespace soma {

class SandboxSpec;

// Extracts values from a SandboxSpec protobuf and exposes
// them in a friendlier format.
class SOMA_EXPORT ReadOnlySandboxSpec {
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

  struct Executable {
   public:
    Executable(std::vector<std::string> command_line_in,
               uid_t uid_in,
               gid_t gid_in,
               base::FilePath working_directory_in,
               bool all_tcp_ports_allowed_in,
               bool all_udp_ports_allowed_in,
               std::vector<uint32_t> tcp_listen_ports_in,
               std::vector<uint32_t> udp_listen_ports_in);
    ~Executable();

    const std::vector<std::string> command_line;
    const uid_t uid;
    const gid_t gid;
    const base::FilePath working_directory;
    const bool all_tcp_ports_allowed;
    const bool all_udp_ports_allowed;
    const std::vector<uint32_t> tcp_listen_ports;
    const std::vector<uint32_t> udp_listen_ports;

   private:
    DISALLOW_COPY_AND_ASSIGN(Executable);
  };
  using ExecutableVector = std::vector<std::unique_ptr<Executable>>;

  ReadOnlySandboxSpec();
  virtual ~ReadOnlySandboxSpec();

  // Returns true if |spec| is well-formed. It is safe to free |spec| as soon
  // as Init() returns.
  // If this method returns false, the caller should refuse to consume
  // the provided SandboxSpec.
  bool Init(const SandboxSpec& spec);

  void Clear();

  const std::string& name() const { return name_; }

  const base::FilePath& overlay_path() const { return overlay_path_; }

  const std::vector<std::string>& endpoint_names() const {
    return endpoint_names_;
  }

  const ExecutableVector& executables() const { return executables_; }

  const std::vector<Namespace>& namespaces() const { return namespaces_; }

  const std::vector<std::pair<int, int>>& device_node_filters() const {
    return device_node_filters_;
  }
  const std::vector<base::FilePath>& device_path_filters() const {
    return device_path_filters_;
  }

  const std::vector<uid_t>& user_acl_for(const std::string& endpoint_name) {
    return (user_acls_.count(endpoint_name) ? user_acls_[endpoint_name]
                                            : empty_user_acl_);
  }
  const std::vector<gid_t>& group_acl_for(const std::string& endpoint_name) {
    return (group_acls_.count(endpoint_name) ? group_acls_[endpoint_name]
                                             : empty_group_acl_);
  }

  base::TimeDelta shutdown_timeout() const { return shutdown_timeout_; }

 private:
  std::string name_;
  base::FilePath overlay_path_;
  std::vector<std::string> endpoint_names_;
  std::vector<Namespace> namespaces_;
  std::vector<base::FilePath> device_path_filters_;
  std::vector<std::pair<int, int>> device_node_filters_;  // major, minor
  std::map<std::string, std::vector<uid_t>> user_acls_;
  std::map<std::string, std::vector<gid_t>> group_acls_;
  ExecutableVector executables_;

  static const std::vector<uid_t> empty_user_acl_;
  static const std::vector<gid_t> empty_group_acl_;

  base::TimeDelta shutdown_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ReadOnlySandboxSpec);
};

}  // namespace soma

#endif  // SOMA_LIB_SOMA_READ_ONLY_SANDBOX_SPEC_H_
