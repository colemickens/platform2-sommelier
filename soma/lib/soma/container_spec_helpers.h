// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_CONTAINER_SPEC_HELPERS_H_
#define SOMA_LIB_SOMA_CONTAINER_SPEC_HELPERS_H_

#include <sys/types.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>

#include "soma/lib/soma/device_filter.h"
#include "soma/lib/soma/namespace.h"
#include "soma/lib/soma/port.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {
namespace parser {
namespace container_spec_helpers {

// Setter helpers for the ContainerSpec protobuf. A lot of these force the
// caller to provide a std::set so stuff's all de-duped on the way in.

std::unique_ptr<ContainerSpec> CreateContainerSpec(const std::string& name);

void SetServiceBundlePath(const base::FilePath& service_bundle_path,
                          ContainerSpec* to_modify);
void SetServiceNames(const std::vector<std::string>& service_names,
                     ContainerSpec* to_modify);
void SetNamespaces(const std::set<ns::Kind>& namespaces,
                   ContainerSpec* to_modify);
void SetDevicePathFilters(const DevicePathFilter::Set& filters,
                          ContainerSpec* to_modify);
void SetDeviceNodeFilters(const DeviceNodeFilter::Set& filters,
                          ContainerSpec* to_modify);

void SetUidAndGid(uid_t uid, gid_t gid, ContainerSpec::Executable* to_modify);
void SetCommandLine(const std::vector<std::string>& command_line,
                    ContainerSpec::Executable* to_modify);
void SetTcpListenPorts(const std::set<port::Number>& ports,
                       ContainerSpec::Executable* to_modify);
void SetUdpListenPorts(const std::set<port::Number>& ports,
                       ContainerSpec::Executable* to_modify);

}  // namespace container_spec_helpers
}  // namespace parser
}  // namespace soma
#endif  // SOMA_LIB_SOMA_CONTAINER_SPEC_HELPERS_H_
