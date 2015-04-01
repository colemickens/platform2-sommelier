// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/registrar.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>
#include <gtest/gtest.h>
#include <libprotobinder/binder_manager.h>
#include <libprotobinder/binder_manager_stub.h>
#include <libprotobinder/binder_proxy.h>
#include <libprotobinder/iinterface.h>
#include <soma/constants.h>

#include "psyche/common/util.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/proto_bindings/soma.pb.h"
#include "psyche/proto_bindings/soma.pb.rpc.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/client_stub.h"
#include "psyche/psyched/service_stub.h"

using protobinder::BinderManagerInterface;
using protobinder::BinderManagerStub;
using protobinder::BinderProxy;
using soma::ContainerSpec;

namespace psyche {
namespace {

// Implementation that returns stub objects.
class TestDelegate : public Registrar::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  // Returns the last-created ServiceStub or ClientStub for the given
  // identifier.
  ServiceStub* GetService(const std::string& service_name) {
    auto const it = services_.find(service_name);
    return it != services_.end() ? it->second : nullptr;
  }
  ClientStub* GetClient(const BinderProxy& client_proxy) {
    auto const it = clients_.find(client_proxy.handle());
    return it != clients_.end() ? it->second : nullptr;
  }

  // Delegate:
  std::unique_ptr<ServiceInterface> CreateService(
      const std::string& name) override {
    ServiceStub* service = new ServiceStub(name);
    services_[name] = service;
    return std::unique_ptr<ServiceInterface>(service);
  }
  std::unique_ptr<ClientInterface> CreateClient(
      std::unique_ptr<BinderProxy> client_proxy) override {
    const uint32_t handle = client_proxy->handle();
    ClientStub* client = new ClientStub(std::move(client_proxy));
    clients_[handle] = client;
    return std::unique_ptr<ClientInterface>(client);
  }

 private:
  // Services and clients that have been returned by CreateService() and
  // CreateClient(). Keyed by service name and client proxy handle,
  // respectively. Objects are owned by Registrar.
  std::map<std::string, ServiceStub*> services_;
  std::map<uint32_t, ClientStub*> clients_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

// Stub implementation of the Soma interface that returns canned ContainerSpecs.
class SomaInterfaceStub : public soma::ISoma {
 public:
  SomaInterfaceStub() = default;
  ~SomaInterfaceStub() override = default;

  void SetContainerSpec(const std::string& service_name,
                        const ContainerSpec& spec) {
    specs_[service_name] = spec;
  }

  // ISoma:
  int GetContainerSpec(soma::GetContainerSpecRequest* in,
                       soma::GetContainerSpecResponse* out) override {
    const auto& it = specs_.find(in->service_name());
    if (it != specs_.end())
      out->mutable_container_spec()->CopyFrom(it->second);
    return 0;
  }

 private:
  std::map<std::string, ContainerSpec> specs_;

  DISALLOW_COPY_AND_ASSIGN(SomaInterfaceStub);
};

class RegistrarTest : public testing::Test {
 public:
  RegistrarTest()
      : binder_manager_(new BinderManagerStub),
        delegate_(new TestDelegate),
        registrar_(new Registrar),
        next_proxy_handle_(1) {
    BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>(binder_manager_));
    registrar_->SetDelegateForTesting(
        std::unique_ptr<Registrar::Delegate>(delegate_));
    registrar_->Init();
  }

  ~RegistrarTest() override {
    // Kill the registrar before uninstalling the BinderManagerStub to ensure
    // that still-open BinderProxy objects don't try to use the now-real
    // BinderManager.
    registrar_.reset();
    BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>());
  }

 protected:
  // Returns a new BinderProxy with a unique handle. Passes a bare pointer since
  // tests will probably want to hang on to the proxy's address while also
  // passing ownership of it to Registrar.
  BinderProxy* CreateBinderProxy() {
    return new BinderProxy(next_proxy_handle_++);
  }

  bool RegisterService(const std::string& service_name,
                       std::unique_ptr<BinderProxy> service_proxy) {
    RegisterServiceRequest register_request;
    register_request.set_name(service_name);
    // |service_proxy| will be extracted from the protocol buffer into another
    // std::unique_ptr, so release it from the original one here.
    util::CopyBinderToProto(*(service_proxy.release()),
                            register_request.mutable_binder());
    RegisterServiceResponse register_response;

    int result = registrar_->RegisterService(&register_request,
                                             &register_response);
    if (result != 0) {
      LOG(FATAL) << "RegisterService call for " << service_name << " returned "
                 << result;
    }
    return register_response.success();
  }

  BinderManagerStub* binder_manager_;  // Not owned.

  TestDelegate* delegate_;  // Owned by |registrar_|.
  std::unique_ptr<Registrar> registrar_;

  // Next handle for CreateBinderProxy() to use.
  uint32_t next_proxy_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistrarTest);
};

TEST_F(RegistrarTest, RegisterAndRequestService) {
  // Register a service.
  const std::string kServiceName("service");
  BinderProxy* service_proxy = CreateBinderProxy();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));

  // Request the service.
  RequestServiceRequest request_request;
  request_request.set_name(kServiceName);
  BinderProxy* client_proxy = CreateBinderProxy();
  util::CopyBinderToProto(*client_proxy,
                          request_request.mutable_client_binder());
  RequestServiceResponse request_response;
  EXPECT_EQ(0, registrar_->RequestService(&request_request, &request_response));
  EXPECT_TRUE(request_response.success());

  // Check that the service was added to the client.
  ClientStub* client = delegate_->GetClient(*client_proxy);
  ASSERT_TRUE(client);
  ServiceStub* service = delegate_->GetService(kServiceName);
  ASSERT_TRUE(service);
  ASSERT_EQ(1U, client->GetServices().count(service));
}

TEST_F(RegistrarTest, ReregisterService) {
  // Register a service.
  const std::string kServiceName("service");
  BinderProxy* service_proxy = CreateBinderProxy();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));

  // The service should be started and hold the correct proxy.
  ServiceStub* service = delegate_->GetService(kServiceName);
  ASSERT_TRUE(service);
  ASSERT_EQ(ServiceInterface::STATE_STARTED, service->GetState());
  EXPECT_EQ(service_proxy, service->GetProxy());

  // Trying to register the same service again while it's still running should
  // fail.
  service_proxy = CreateBinderProxy();
  EXPECT_FALSE(RegisterService(kServiceName,
                               std::unique_ptr<BinderProxy>(service_proxy)));

  // After stopping the service, it should be possible to register it again.
  service->set_state(ServiceInterface::STATE_STOPPED);
  service_proxy = CreateBinderProxy();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));
  EXPECT_EQ(ServiceInterface::STATE_STARTED, service->GetState());
  EXPECT_EQ(service_proxy, service->GetProxy());
}

TEST_F(RegistrarTest, RegisterSoma) {
  SomaInterfaceStub* interface = new SomaInterfaceStub;
  BinderProxy* soma_proxy = CreateBinderProxy();
  binder_manager_->SetTestInterface(
      soma_proxy, scoped_ptr<IInterface>(interface));

  EXPECT_TRUE(RegisterService(soma::kSomaServiceName,
                              std::unique_ptr<BinderProxy>(soma_proxy)));

  const std::string kServiceName("org.example.myservice");
  interface->SetContainerSpec(kServiceName, ContainerSpec());

  RequestServiceRequest request_request;
  request_request.set_name(kServiceName);
  BinderProxy* client_proxy = CreateBinderProxy();
  util::CopyBinderToProto(*client_proxy,
                          request_request.mutable_client_binder());
  RequestServiceResponse request_response;
  EXPECT_EQ(0, registrar_->RequestService(&request_request, &request_response));

  // TODO(derat): Check that the container is started and that after its service
  // is registered the service gets passed to the client.
}

}  // namespace
}  // namespace psyche
