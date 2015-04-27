// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/cell.h"

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
  GermInterfaceStub()
      : launch_return_value_(0),
        terminate_return_value_(0),
        launch_success_(true),
        terminate_success_(true) {}
  ~GermInterfaceStub() override = default;

  void set_launch_return_value(int value) { launch_return_value_ = value; }
  void set_launch_pid(int pid) {
    launch_pid_ = pid;
  }
  void set_terminate_return_value(int value) {
    terminate_return_value_ = value;
  }
  const std::vector<std::string>& launched_cell_names() const {
    return launched_cell_names_;
  }
  const std::vector<int>& terminated_cell_pids() const {
    return terminated_cell_pids_;
  }

  // IGerm:
  int Launch(germ::LaunchRequest* in, germ::LaunchResponse* out) override {
    out->set_success(launch_success_);
    out->set_pid(launch_pid_);
    launched_cell_names_.push_back(in->spec().name());
    return launch_return_value_;
  }

  int Terminate(germ::TerminateRequest* in,
                germ::TerminateResponse* out) override {
    out->set_success(terminate_success_);
    terminated_cell_pids_.push_back(in->pid());
    return terminate_return_value_;
  }

 private:
  // binder result returned by Launch().
  int launch_return_value_;

  // binder result returned by Terminate().
  int terminate_return_value_;

  // germ success field returned by LaunchResponse.
  bool launch_success_;

  // germ pid field returned by LaunchResponse.
  int launch_pid_;

  // germ success field returned by TerminateResponse.
  bool terminate_success_;

  // Cell names passed to Launch().
  std::vector<std::string> launched_cell_names_;

  // Cell init PIDs passed to Terminate().
  std::vector<int> terminated_cell_pids_;

  DISALLOW_COPY_AND_ASSIGN(GermInterfaceStub);
};

class CellTest : public BinderTestBase {
 public:
  CellTest()
      : germ_proxy_(nullptr),
        germ_(nullptr) {
    InitGerm();
  }
  ~CellTest() override = default;

 protected:
  // Initializes |germ_proxy_| and |germ_| and registers them with
  // |binder_manager_|.
  void InitGerm() {
    germ_proxy_ = CreateBinderProxy().release();
    germ_ = new GermInterfaceStub;
    binder_manager_->SetTestInterface(germ_proxy_,
                                      std::unique_ptr<IInterface>(germ_));
    germ_connection_.SetProxy(std::unique_ptr<BinderProxy>(germ_proxy_));
  }

  StubFactory factory_;
  GermConnection germ_connection_;
  BinderProxy* germ_proxy_;  // Owned by this class.
  GermInterfaceStub* germ_;  // Owned by |binder_manager_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(CellTest);
};

TEST_F(CellTest, InitializeFromSpec) {
  ContainerSpec spec;
  const std::string kCellName("/tmp/org.example.cell");
  spec.set_name(kCellName);
  const std::vector<std::string> kServiceNames = {
    "org.example.search.query",
    "org.example.search.autocomplete",
  };
  for (const auto& name : kServiceNames)
    spec.add_service_names(name);

  Cell cell(spec, &factory_, &germ_connection_);
  EXPECT_EQ(kCellName, cell.GetName());

  // Check that all of the listed services were created.
  const CellInterface::ServiceMap& services = cell.GetServices();
  for (const auto& name : kServiceNames) {
    SCOPED_TRACE(name);
    const auto it = services.find(name);
    ASSERT_TRUE(it != services.end());
    EXPECT_EQ(factory_.GetService(name), it->second.get());
  }

  // Modify the spec that was passed in to verify that Cell made its own copy of
  // it.
  spec.set_name("/some/other/name");
  EXPECT_EQ(kCellName, cell.GetName());
}

// Tests various failures when communicating with germd.
TEST_F(CellTest, GermCommunication) {
  ContainerSpec spec;
  const std::string kCellName("/tmp/org.example.cell");
  const int kCellPid(123);
  spec.set_name(kCellName);
  germ_->set_launch_pid(kCellPid);

  Cell cell(spec, &factory_, &germ_connection_);

  // Failure should be reported for RPC errors.
  germ_->set_launch_return_value(-1);
  EXPECT_FALSE(cell.Launch());

  // Now report that the germd binder proxy died.
  germ_->set_launch_return_value(0);
  binder_manager_->ReportBinderDeath(germ_proxy_);
  EXPECT_FALSE(cell.Launch());

  // Register a new proxy for germd and check that the next service request is
  // successful.
  InitGerm();
  EXPECT_TRUE(cell.Launch());
  ASSERT_EQ(1, germ_->launched_cell_names().size());
  EXPECT_EQ(kCellName, germ_->launched_cell_names()[0]);

  // Make similar calls to Terminate() as Launch(), showing failures for RPC,
  // binder death, and restart and success.
  germ_->set_terminate_return_value(-1);
  EXPECT_FALSE(cell.Terminate());

  germ_->set_terminate_return_value(0);
  binder_manager_->ReportBinderDeath(germ_proxy_);
  EXPECT_FALSE(cell.Terminate());

  InitGerm();
  EXPECT_TRUE(cell.Terminate());
  ASSERT_EQ(1, germ_->terminated_cell_pids().size());
  EXPECT_EQ(kCellPid, germ_->terminated_cell_pids()[0]);
}

}  // namespace
}  // namespace psyche
