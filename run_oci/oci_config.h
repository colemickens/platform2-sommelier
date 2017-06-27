// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Container configuration from the config.json data as specified in
// https://github.com/opencontainers/runtime-spec/tree/v1.0.0-rc2

#ifndef RUN_OCI_OCI_CONFIG_H_
#define RUN_OCI_OCI_CONFIG_H_

#include <stdint.h>

#include <string>
#include <vector>

struct OciPlatform {
  std::string os;
  std::string arch;
};

struct OciProcessUser {
  uint32_t uid;
  uint32_t gid;
  std::vector<uint32_t> additionalGids;  // Optional
};

struct OciProcess {
  bool terminal;  // Optional
  OciProcessUser user;
  std::vector<std::string> args;
  std::vector<std::string> env;  // Optional
  std::string cwd;
  // Unused: capabilities, rlimits, apparmorProfile,
  //    selinuxLabel, noNewPrivileges
};

struct OciRoot {
  std::string path;
  bool readonly;  // Optional
};

struct OciMount {
  std::string destination;
  std::string type;
  std::string source;
  std::vector<std::string> options;  // Optional
};

struct OciLinuxNamespaceMapping {
  uint32_t hostID;
  uint32_t containerID;
  uint32_t size;
};

struct OciLinuxDevice {
  std::string type;
  std::string path;
  uint32_t major;  // Optional
  uint32_t minor;  // Optional
  uint32_t fileMode;  // Optional
  uint32_t uid;  // Optional
  uint32_t gid;  // Optional
};

struct OciSeccompArg {
  uint32_t index;
  uint64_t value;
  uint64_t value2;
  std::string op;
};

struct OciSeccompSyscall {
  std::string name;
  std::string action;
  std::vector<OciSeccompArg> args;  // Optional
};

struct OciLinuxCgroupDevice {
    bool allow;
    std::string access;  // Optional
    std::string type;  // Optional
    uint32_t major;  // Optional
    uint32_t minor;  // Optional
};

struct OciLinuxResources {
    std::vector<OciLinuxCgroupDevice> devices;
    // Other fields remain unused.
};

struct OciSeccomp {
  std::string defaultAction;
  std::vector<std::string> architectures;
  std::vector<OciSeccompSyscall> syscalls;
};

struct OciLinux {
  std::vector<OciLinuxDevice> devices;  // Optional
  std::string cgroupsPath;  // Optional
  // Unused: namespaces
  OciLinuxResources resources;  // Optional
  std::vector<OciLinuxNamespaceMapping> uidMappings;  // Optional
  std::vector<OciLinuxNamespaceMapping> gidMappings;  // Optional
  OciSeccomp seccomp;  // Optional
  // Unused: maskedPaths, readonlyPaths, rootfsPropagation, mountLabel, sysctl
};

struct OciConfig {
  std::string ociVersion;
  OciPlatform platform;
  OciRoot root;
  OciProcess process;
  std::string hostname;  // Optional
  std::vector<OciMount> mounts;  // Optional
  // json field name - linux
  OciLinux linux_config;  // Optional
  // Unused: hooks, annotations
};

#endif  // RUN_OCI_OCI_CONFIG_H_
