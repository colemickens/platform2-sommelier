// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/cell.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <chromeos/make_unique_ptr.h>
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

using chromeos::make_unique_ptr;
using protobinder::BinderProxy;
using soma::ContainerSpec;

namespace psyche {
namespace {

// Stub implementation of the Germ interface.
class GermInterfaceStub : public germ::IGerm {
 public:
  GermInterfaceStub()
      : launch_return_value_(0) {}
  ~GermInterfaceStub() override = default;

  void set_launch_return_value(int value) { launch_return_value_ = value; }
  const std::vector<std::string>& launched_cell_names() const {
    return launched_cell_names_;
  }

  // IGerm:
  Status Launch(germ::LaunchRequest* in, germ::LaunchResponse* out) override {
    launched_cell_names_.push_back(in->spec().name());
    return launch_return_value_
               ? STATUS_APP_ERROR(launch_return_value_, "Launch Error")
               : STATUS_OK();
  }

  Status Terminate(germ::TerminateRequest* in,
                   germ::TerminateResponse* out) override {
    return STATUS_OK();
  }

 private:
  // binder result returned by Launch().
  int launch_return_value_;

  // Cell names passed to Launch().
  std::vector<std::string> launched_cell_names_;

  DISALLOW_COPY_AND_ASSIGN(GermInterfaceStub);
};

class CellTest : public BinderTestBase {
 public:
  CellTest() : germ_handle_(0), germ_(nullptr) {
    InitGerm();
  }
  ~CellTest() override = default;

 protected:
  // Initializes |germ_handle_| and |germ_| and registers them with
  // |binder_manager_|.
  void InitGerm() {
    germ_handle_ = CreateBinderProxyHandle();
    germ_ = new GermInterfaceStub;
    binder_manager_->SetTestInterface(germ_handle_,
                                      std::unique_ptr<IInterface>(germ_));
    germ_connection_.SetProxy(make_unique_ptr(new BinderProxy(germ_handle_)));
  }

  StubFactory factory_;
  GermConnection germ_connection_;
  uint32_t germ_handle_;
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
  spec.set_name(kCellName);

  Cell cell(spec, &factory_, &germ_connection_);

  // Failure should be reported for RPC errors.
  germ_->set_launch_return_value(-1);
  EXPECT_FALSE(cell.Launch());

  // Now report that the germd binder proxy died.
  germ_->set_launch_return_value(0);
  binder_manager_->ReportBinderDeath(germ_handle_);
  EXPECT_FALSE(cell.Launch());

  // Register a new proxy for germd and check that the next service request is
  // successful.
  InitGerm();
  EXPECT_TRUE(cell.Launch());
  ASSERT_EQ(1, germ_->launched_cell_names().size());
  EXPECT_EQ(kCellName, germ_->launched_cell_names()[0]);
}

// Tests notifying services that don't register themselves.
TEST_F(CellTest, VerifyServiceRegistration) {
  ContainerSpec spec;
  const std::vector<std::string> kServiceNames = {
    "org.example.search.query",
    "org.example.search.autocomplete",
  };
  for (const auto& name : kServiceNames)
    spec.add_service_names(name);

  const std::string kCellName("/tmp/org.example.cell");
  spec.set_name(kCellName);

  Cell cell(spec, &factory_, &germ_connection_);
  Cell::TestApi test_api(&cell);

  // The timer should not be scheduled yet.
  EXPECT_FALSE(test_api.TriggerVerifyServicesTimeout());

  EXPECT_TRUE(cell.Launch());

  // Trigger the timeout before the services register.
  EXPECT_TRUE(test_api.TriggerVerifyServicesTimeout());
  // After that, we give up and don't set the timer again.
  EXPECT_FALSE(test_api.TriggerVerifyServicesTimeout());
  // The services have been notified of the failure.
  for (const auto& service_name : kServiceNames) {
    ServiceStub* service = factory_.GetService(service_name);
    EXPECT_EQ(1, service->GetAndResetOnServiceUnavailableCount())
        << service_name << " had wrong unavailable count";
  }

  // Start again and make the services register.
  InitGerm();
  EXPECT_TRUE(cell.Launch());

  for (const auto& service_it : cell.GetServices()) {
    BinderProxy* service_proxy = CreateBinderProxy().release();
    service_it.second->SetProxy(std::unique_ptr<BinderProxy>(service_proxy));
    // Manually call OnServiceProxyChange() because the ServiceStub class
    // doesn't notify its observers of proxy changes.
    cell.OnServiceProxyChange(service_it.second.get());
  }

  // The cell cleared the timer because all services registered in time.
  EXPECT_FALSE(test_api.TriggerVerifyServicesTimeout());

  // OnServiceUnavailable() hasn't been called on any of the services.
  for (const auto& service_name : kServiceNames) {
    ServiceStub* service = factory_.GetService(service_name);
    EXPECT_EQ(0, service->GetAndResetOnServiceUnavailableCount())
        << service_name << " had wrong unavailable count";
  }
}

}  // namespace
}  // namespace psyche
