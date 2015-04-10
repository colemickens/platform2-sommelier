// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/container.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <gtest/gtest.h>
#include <protobinder/binder_manager_stub.h>
#include <protobinder/binder_proxy.h>

#include "psyche/common/binder_test_base.h"
#include "psyche/proto_bindings/germ.pb.h"
#include "psyche/proto_bindings/germ.pb.rpc.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/germ_connection.h"
#include "psyche/psyched/service_stub.h"
#include "psyche/psyched/stub_factory.h"

using protobinder::BinderProxy;
using soma::ContainerSpec;

namespace psyche {
namespace {

// Stub implementation of the Germ interface.
class GermInterfaceStub : public germ::IGerm {
 public:
  GermInterfaceStub() : launch_return_value_(0), terminate_return_value_(0) {}
  ~GermInterfaceStub() override = default;

  void set_launch_return_value(int value) { launch_return_value_ = value; }
  void set_terminate_return_value(int value) {
    terminate_return_value_ = value;
  }

  // IGerm:
  int Launch(germ::LaunchRequest* in, germ::LaunchResponse* out) override {
    return launch_return_value_;
  }

  int Terminate(germ::TerminateRequest* in,
                germ::TerminateResponse* out) override {
    return terminate_return_value_;
  }

 private:
  // binder result returned by Launch().
  int launch_return_value_;

  // binder result returned by Terminate().
  int terminate_return_value_;

  DISALLOW_COPY_AND_ASSIGN(GermInterfaceStub);
};

class ContainerTest : public BinderTestBase {
 public:
  ContainerTest()
      : germ_proxy_(nullptr),
        germ_(nullptr) {
    InitGerm();
  }
  ~ContainerTest() override = default;

 protected:
  // Initializes |germ_proxy_| and |germ_| and registers them with
  // |binder_manager_|.
  void InitGerm() {
    germ_proxy_ = CreateBinderProxy().release();
    germ_ = new GermInterfaceStub;
    binder_manager_->SetTestInterface(germ_proxy_,
                                      scoped_ptr<IInterface>(germ_));
    germ_connection_.SetProxy(std::unique_ptr<BinderProxy>(germ_proxy_));
  }

  StubFactory factory_;
  GermConnection germ_connection_;
  BinderProxy* germ_proxy_;  // Owned by this class.
  GermInterfaceStub* germ_;  // Owned by |binder_manager_|.

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

  Container container(spec, &factory_, &germ_connection_);
  EXPECT_EQ(kContainerName, container.GetName());

  // Check that all of the listed services were created.
  const ContainerInterface::ServiceMap& services = container.GetServices();
  for (const auto& name : kServiceNames) {
    SCOPED_TRACE(name);
    const auto it = services.find(name);
    ASSERT_TRUE(it != services.end());
    EXPECT_EQ(factory_.GetService(name), it->second.get());
  }

  // Modify the spec that was passed in to verify that Container made its own
  // copy of it.
  spec.set_name("/some/other/name");
  EXPECT_EQ(kContainerName, container.GetName());
}

}  // namespace
}  // namespace psyche
