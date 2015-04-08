// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/client.h"

#include <cstdint>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <protobinder/binder_manager_stub.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iinterface.h>

#include "psyche/common/binder_test_base.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/psyched/client_stub.h"
#include "psyche/psyched/service_stub.h"

using protobinder::BinderProxy;

namespace psyche {
namespace {

// Stub implementation of IPsycheClient that just logs the service names and
// binder handles that it's instructed to send.
class PsycheClientInterfaceStub : public IPsycheClient {
 public:
  PsycheClientInterfaceStub() = default;
  ~PsycheClientInterfaceStub() override = default;

  // Pairs are [service_name, proxy_handle].
  using ServiceHandle = std::pair<std::string, uint64_t>;
  using ServiceHandles = std::vector<ServiceHandle>;

  const ServiceHandles& service_handles() const { return service_handles_; }
  void clear_service_handles() { service_handles_.clear(); }

  // IPsycheClient:
  int ReceiveService(ReceiveServiceRequest* in,
                     ReceiveServiceResponse* out) override {
    service_handles_.push_back(
        std::make_pair(in->name(), in->binder().ibinder()));
    return 0;
  }

 private:
  ServiceHandles service_handles_;

  DISALLOW_COPY_AND_ASSIGN(PsycheClientInterfaceStub);
};

using ClientTest = BinderTestBase;

TEST_F(ClientTest, PassServiceHandles) {
  PsycheClientInterfaceStub* interface = new PsycheClientInterfaceStub;
  BinderProxy* client_proxy = CreateBinderProxy().release();
  binder_manager_->SetTestInterface(
      client_proxy, scoped_ptr<IInterface>(interface));
  Client client((std::unique_ptr<BinderProxy>(client_proxy)));

  // Adding a not-yet-started service shouldn't send anything.
  const std::string kServiceName("stub");
  ServiceStub service(kServiceName);
  client.AddService(&service);
  ASSERT_EQ(0U, interface->service_handles().size());

  // Start the service and check that its handle is sent.
  BinderProxy* service_proxy = CreateBinderProxy().release();
  service.SetProxyForTesting(std::unique_ptr<BinderProxy>(service_proxy));
  client.OnServiceProxyChange(&service);
  ASSERT_EQ(1U, interface->service_handles().size());
  EXPECT_EQ(kServiceName, interface->service_handles()[0].first);
  EXPECT_EQ(reinterpret_cast<uint64_t>(service_proxy),
            interface->service_handles()[0].second);
  interface->clear_service_handles();

  // Stop the service. Nothing should be sent until it's started again.
  service.SetProxyForTesting(std::unique_ptr<BinderProxy>());
  client.OnServiceProxyChange(&service);
  ASSERT_EQ(0U, interface->service_handles().size());
  service_proxy = CreateBinderProxy().release();
  service.SetProxyForTesting(std::unique_ptr<BinderProxy>(service_proxy));
  client.OnServiceProxyChange(&service);
  ASSERT_EQ(1U, interface->service_handles().size());
  EXPECT_EQ(kServiceName, interface->service_handles()[0].first);
  EXPECT_EQ(reinterpret_cast<uint64_t>(service_proxy),
            interface->service_handles()[0].second);
  interface->clear_service_handles();

  // Add a second already-running service.
  const std::string kService2Name("stub2");
  ServiceStub service2(kService2Name);
  BinderProxy* service2_proxy = CreateBinderProxy().release();
  service2.SetProxyForTesting(std::unique_ptr<BinderProxy>(service2_proxy));
  client.AddService(&service2);
  ASSERT_EQ(1U, interface->service_handles().size());
  EXPECT_EQ(kService2Name, interface->service_handles()[0].first);
  EXPECT_EQ(reinterpret_cast<uint64_t>(service2_proxy),
            interface->service_handles()[0].second);

  client.RemoveService(&service);
  client.RemoveService(&service2);
}

}  // namespace
}  // namespace psyche
