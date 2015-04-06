// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/read_only_container_spec.h"

#include <algorithm>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <gtest/gtest.h>

#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {

class ReadOnlyContainerSpecTest : public ::testing::Test {
 public:
  ReadOnlyContainerSpecTest() {}
  virtual ~ReadOnlyContainerSpecTest() {}

 protected:
  ContainerSpec spec_;
};

TEST_F(ReadOnlyContainerSpecTest, RequiredFieldsTest) {
  const char fully_qualified_spec_name[] = "/path/to/spec.json";
  const char service_bundle_path[] = "/path/to/bundle";
  const uid_t uid = 1;
  const gid_t gid = 8;
  const char* command_line[2] = { "command", "arg1" };

  spec_.set_name(fully_qualified_spec_name);
  spec_.set_service_bundle_path(service_bundle_path);
  spec_.set_uid(uid);
  spec_.set_gid(gid);
  spec_.add_command_line(command_line[0]);
  spec_.add_command_line(command_line[1]);

  ReadOnlyContainerSpec ro_spec(&spec_);
  EXPECT_EQ(ro_spec.name(), fully_qualified_spec_name);
  EXPECT_EQ(ro_spec.service_bundle_path().value(), service_bundle_path);
  EXPECT_EQ(ro_spec.uid(), uid);
  EXPECT_EQ(ro_spec.gid(), gid);
  const std::vector<std::string> cl(ro_spec.command_line());
  EXPECT_TRUE(std::equal(cl.begin(), cl.end(), std::begin(command_line)));
}

TEST_F(ReadOnlyContainerSpecTest, WorkingDirectoryTest) {
  const char working_directory[] = "/working/directory";
  spec_.set_working_directory(working_directory);
  ReadOnlyContainerSpec ro_spec(&spec_);
  EXPECT_EQ(ro_spec.working_directory().value(), working_directory);
}

TEST_F(ReadOnlyContainerSpecTest, ServiceNamesTest) {
  const char* service_names[2] = { "name1", "name2" };
  spec_.add_service_names(service_names[0]);
  spec_.add_service_names(service_names[1]);
  ReadOnlyContainerSpec ro_spec(&spec_);

  const std::vector<std::string> names(ro_spec.service_names());
  EXPECT_TRUE(std::equal(names.begin(), names.end(),
                         std::begin(service_names)));
}

TEST_F(ReadOnlyContainerSpecTest, NamespacesTest) {
  ContainerSpec::Namespace namespaces_in[] = { ContainerSpec::NEWIPC,
                                               ContainerSpec::NEWUSER };
  ReadOnlyContainerSpec::Namespace namespaces_out[] = {
    ReadOnlyContainerSpec::Namespace::NEWIPC,
    ReadOnlyContainerSpec::Namespace::NEWUSER
  };
  spec_.add_namespaces(namespaces_in[0]);
  spec_.add_namespaces(namespaces_in[1]);

  ReadOnlyContainerSpec ro_spec(&spec_);
  const std::vector<ReadOnlyContainerSpec::Namespace> ns(ro_spec.namespaces());
  EXPECT_EQ(ns.size(), arraysize(namespaces_out));
  for (size_t i = 0; i < arraysize(namespaces_out); ++i)
    EXPECT_NE(std::find(ns.begin(), ns.end(), namespaces_out[i]), ns.end());
}

TEST_F(ReadOnlyContainerSpecTest, ListenPortsTest) {
  {
    ReadOnlyContainerSpec ro_spec(&spec_);
    EXPECT_FALSE(ro_spec.all_tcp_ports_allowed());
    EXPECT_FALSE(ro_spec.all_udp_ports_allowed());
  }
  uint32_t tcp_ports[] = { 80, 8080, 1337 };
  spec_.mutable_tcp_listen_ports()->add_ports(tcp_ports[0]);
  spec_.mutable_tcp_listen_ports()->add_ports(tcp_ports[1]);
  spec_.mutable_tcp_listen_ports()->add_ports(tcp_ports[2]);
  spec_.mutable_udp_listen_ports()->set_allow_all(true);

  ReadOnlyContainerSpec ro_spec(&spec_);
  EXPECT_TRUE(ro_spec.all_udp_ports_allowed());
  EXPECT_FALSE(ro_spec.all_tcp_ports_allowed());
  const std::vector<uint32_t> ports(ro_spec.tcp_listen_ports());
  EXPECT_EQ(ports.size(), arraysize(tcp_ports));
  for (size_t i = 0; i < arraysize(tcp_ports); ++i)
    EXPECT_NE(std::find(ports.begin(), ports.end(), tcp_ports[i]), ports.end());
}

TEST_F(ReadOnlyContainerSpecTest, DeviceFilterTest) {
  const int node_filters[3][2] = { { 1, 1 }, { 2, -1 } , { -1, 0 } };
  for (size_t i = 0; i < arraysize(node_filters); ++i) {
    spec_.add_device_node_filters()->set_major(node_filters[i][0]);
    spec_.mutable_device_node_filters(i)->set_minor(node_filters[i][1]);
  }
  const char* path_filters[2] = { "/foo/bar", "/bar/baz" };
  for (size_t i = 0; i < arraysize(path_filters); ++i)
    spec_.add_device_path_filters()->set_filter(path_filters[i]);

  ReadOnlyContainerSpec ro_spec(&spec_);
  const std::vector<std::pair<int, int>> nodes(ro_spec.device_node_filters());
  for (size_t i = 0; i < arraysize(node_filters); ++i) {
    EXPECT_NE(std::find(nodes.begin(), nodes.end(),
                        std::make_pair(node_filters[i][0], node_filters[i][1])),
              nodes.end());
  }
  const std::vector<base::FilePath> paths(ro_spec.device_path_filters());
  for (size_t i = 0; i < arraysize(path_filters); ++i) {
    EXPECT_NE(std::find(paths.begin(), paths.end(),
                        base::FilePath(path_filters[i])),
              paths.end());
  }
}

}  // namespace soma
