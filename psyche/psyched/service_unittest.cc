// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/service.h"

#include <memory>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <libprotobinder/binder_manager.h>
#include <libprotobinder/binder_manager_stub.h>
#include <libprotobinder/binder_proxy.h>

#include "psyche/psyched/service_observer.h"

using protobinder::BinderManagerInterface;
using protobinder::BinderManagerStub;
using protobinder::BinderProxy;

namespace psyche {
namespace {

class TestObserver : public ServiceObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  using ServiceVector = std::vector<ServiceInterface*>;
  const ServiceVector& changed_services() const { return changed_services_; }
  void clear_changed_services() { changed_services_.clear(); }

  // ServiceObserver:
  void OnServiceStateChange(ServiceInterface* service) override {
    changed_services_.push_back(service);
  }

 private:
  ServiceVector changed_services_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class ServiceTest : public testing::Test {
 public:
  ServiceTest() {
    binder_manager_ = new BinderManagerStub;
    BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>(binder_manager_));
  }
  ~ServiceTest() override {
    BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>());
  }

 protected:
  BinderManagerStub* binder_manager_;  // Not owned.

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceTest);
};

TEST_F(ServiceTest, NotifyObserversAboutStateChanges) {
  const std::string kServiceName("service");
  Service service(kServiceName);
  EXPECT_EQ(ServiceInterface::STATE_STOPPED, service.GetState());

  TestObserver observer;
  service.AddObserver(&observer);

  // Pass the service proxy and check that the service is marked started and
  // that the observer is notified.
  BinderProxy* service_proxy = new BinderProxy(1);
  service.SetProxy(std::unique_ptr<BinderProxy>(service_proxy));
  EXPECT_EQ(ServiceInterface::STATE_STARTED, service.GetState());
  ASSERT_EQ(1U, observer.changed_services().size());
  EXPECT_EQ(&service, observer.changed_services()[0]);
  observer.clear_changed_services();

  // Killing the proxy should result in the service stopping and the observer
  // being notified again.
  binder_manager_->ReportBinderDeath(service_proxy);
  EXPECT_EQ(ServiceInterface::STATE_STOPPED, service.GetState());
  ASSERT_EQ(1U, observer.changed_services().size());
  EXPECT_EQ(&service, observer.changed_services()[0]);

  service.RemoveObserver(&observer);
}

}  // namespace
}  // namespace psyche
