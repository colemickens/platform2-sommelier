// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/service.h"

#include <string>

#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <libprotobinder/binder_manager.h>
#include <libprotobinder/binder_manager_stub.h>
#include <libprotobinder/binder_proxy.h>

#include "psyche/psyched/client_stub.h"

using protobinder::BinderManagerInterface;
using protobinder::BinderManagerStub;
using protobinder::BinderProxy;

namespace psyche {
namespace {

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

TEST_F(ServiceTest, NotifyClientsAboutStateChanges) {
  const std::string kServiceName("service");
  Service service(kServiceName);
  EXPECT_EQ(ServiceInterface::STATE_STOPPED, service.GetState());

  uint32_t next_proxy_handle = 1;
  BinderProxy* client_proxy = new BinderProxy(next_proxy_handle++);
  ClientStub client(make_scoped_ptr(client_proxy));
  service.AddClient(&client);

  // Pass the service proxy and check that the service is marked started and
  // that the client is notified.
  BinderProxy* service_proxy = new BinderProxy(next_proxy_handle++);
  service.SetProxy(make_scoped_ptr(service_proxy));
  EXPECT_EQ(ServiceInterface::STATE_STARTED, service.GetState());
  ASSERT_EQ(1U, client.services_with_changed_states().size());
  EXPECT_EQ(&service, client.services_with_changed_states()[0]);
  client.clear_services_with_changed_states();

  // Killing the proxy should result in the service stopping.
  binder_manager_->ReportBinderDeath(service_proxy);
  EXPECT_EQ(ServiceInterface::STATE_STOPPED, service.GetState());
  ASSERT_EQ(1U, client.services_with_changed_states().size());
  EXPECT_EQ(&service, client.services_with_changed_states()[0]);

  service.RemoveClient(&client);
}

}  // namespace
}  // namespace psyche
