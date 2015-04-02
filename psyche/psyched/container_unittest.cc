// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/container.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <gtest/gtest.h>

#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/service_stub.h"
#include "psyche/psyched/stub_factory.h"

using soma::ContainerSpec;

namespace psyche {
namespace {

class ContainerTest : public testing::Test {
 public:
  ContainerTest() = default;
  ~ContainerTest() override = default;

 protected:
  StubFactory factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerTest);
};

TEST_F(ContainerTest, InitializeFromSpec) {
  ContainerSpec spec;
  const std::string kContainerName("/tmp/org.example.container");
  spec.set_name(kContainerName);
  const std::vector<std::string> kServiceNames = {
    "org.example.search.query",
    "org.example.search.autocomplete",
  };
  for (const auto& name : kServiceNames)
    spec.add_service_names(name);

  Container container(spec, &factory_);
  EXPECT_EQ(kContainerName, container.GetName());

  // Check that all of the listed services were created.
  const ContainerInterface::ServiceMap& services = container.GetServices();
  for (const auto& name : kServiceNames) {
    SCOPED_TRACE(name);
    const auto it = services.find(name);
    ASSERT_TRUE(it != services.end());
    EXPECT_EQ(factory_.GetService(name), it->second.get());
  }
}

}  // namespace
}  // namespace psyche
