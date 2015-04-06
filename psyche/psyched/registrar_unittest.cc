// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/psyched/registrar.h"

#include <memory>
#include <string>
#include <utility>

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
#include "psyche/psyched/container_stub.h"
#include "psyche/psyched/service_stub.h"
#include "psyche/psyched/stub_factory.h"

using protobinder::BinderManagerInterface;
using protobinder::BinderManagerStub;
using protobinder::BinderProxy;
using soma::ContainerSpec;

namespace psyche {
namespace {

// Stub implementation of the Soma interface that returns canned ContainerSpecs.
class SomaInterfaceStub : public soma::ISoma {
 public:
  SomaInterfaceStub() : return_value_(0) {}
  ~SomaInterfaceStub() override = default;

  void set_return_value(int value) { return_value_ = value; }

  // Sets the ContainerSpec to return in response to a request for
  // |service_name|.
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
    return return_value_;
  }

 private:
  std::map<std::string, ContainerSpec> specs_;

  // binder result returned by GetContainerSpec().
  int return_value_;

  DISALLOW_COPY_AND_ASSIGN(SomaInterfaceStub);
};

class RegistrarTest : public testing::Test {
 public:
  RegistrarTest()
      : binder_manager_(new BinderManagerStub),
        factory_(new StubFactory),
        registrar_(new Registrar),
        soma_proxy_(nullptr),
        soma_(nullptr),
        next_proxy_handle_(1) {
    BinderManagerInterface::SetForTesting(
        scoped_ptr<BinderManagerInterface>(binder_manager_));
    registrar_->SetFactoryForTesting(
        std::unique_ptr<FactoryInterface>(factory_));
    registrar_->Init();
    CHECK(InitSoma());
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
  BinderProxy* CreateBinderProxy() WARN_UNUSED_RESULT {
    return new BinderProxy(next_proxy_handle_++);
  }

  // Initializes |soma_proxy_| and |soma_| and registers them with |registrar_|
  // and |binder_manager_|. May be called from within a test to simulate somad
  // restarting and reregistering itself with psyched.
  bool InitSoma() WARN_UNUSED_RESULT {
    soma_proxy_ = CreateBinderProxy();
    soma_ = new SomaInterfaceStub;
    binder_manager_->SetTestInterface(soma_proxy_,
                                      scoped_ptr<IInterface>(soma_));
    return RegisterService(soma::kSomaServiceName,
                           std::unique_ptr<BinderProxy>(soma_proxy_));
  }

  // Calls |registrar_|'s RegisterService method, returning true on success.
  bool RegisterService(
      const std::string& service_name,
      std::unique_ptr<BinderProxy> service_proxy) WARN_UNUSED_RESULT {
    RegisterServiceRequest request;
    request.set_name(service_name);
    // |service_proxy| will be extracted from the protocol buffer into another
    // std::unique_ptr, so release it from the original one here.
    util::CopyBinderToProto(*(service_proxy.release()),
                            request.mutable_binder());

    RegisterServiceResponse response;
    CHECK_EQ(registrar_->RegisterService(&request, &response), 0)
        << "RegisterService call for " << service_name << " failed";
    return response.success();
  }

  // Calls |registrar_|'s RequestService method, returning true on success.
  bool RequestService(
      const std::string& service_name,
      std::unique_ptr<BinderProxy> client_proxy) WARN_UNUSED_RESULT {
    RequestServiceRequest request;
    request.set_name(service_name);
    // |service_proxy| will be extracted from the protocol buffer into another
    // std::unique_ptr, so release it from the original one here.
    util::CopyBinderToProto(*(client_proxy.release()),
                            request.mutable_client_binder());

    RequestServiceResponse response;
    CHECK_EQ(registrar_->RequestService(&request, &response), 0)
        << "RequestService call for " << service_name << " failed";
    return response.success();
  }

  // Creates a ContainerStub object named |container_name| and registers it in
  // |soma_| and |factory_| so it'll be returned for a request for
  // |service_name|. The caller is responsible for calling the stub's
  // AddService() method to make it claim to provide services.
  //
  // The returned object is owned by |registrar_| (and may not
  // persist beyond the request if |registrar_| decides not to keep it).
  ContainerStub* AddContainer(const std::string& container_name,
                              const std::string& service_name) {
    ContainerSpec spec;
    spec.set_name(container_name);
    soma_->SetContainerSpec(service_name, spec);
    ContainerStub* container = new ContainerStub(container_name);
    factory_->SetContainer(container_name,
                           std::unique_ptr<ContainerStub>(container));
    return container;
  }

  BinderManagerStub* binder_manager_;  // Not owned.

  StubFactory* factory_;  // Owned by |registrar_|.
  std::unique_ptr<Registrar> registrar_;

  BinderProxy* soma_proxy_;  // Owned by |registrar_|.
  SomaInterfaceStub* soma_;  // Owned by |binder_manager_|.

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
  BinderProxy* client_proxy = CreateBinderProxy();
  EXPECT_TRUE(RequestService(kServiceName,
                             std::unique_ptr<BinderProxy>(client_proxy)));

  // Check that the service was added to the client.
  ClientStub* client = factory_->GetClient(*client_proxy);
  ASSERT_TRUE(client);
  ServiceStub* service = factory_->GetService(kServiceName);
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
  ServiceStub* service = factory_->GetService(kServiceName);
  ASSERT_TRUE(service);
  ASSERT_EQ(ServiceInterface::State::STARTED, service->GetState());
  EXPECT_EQ(service_proxy, service->GetProxy());

  // Trying to register the same service again while it's still running should
  // fail.
  service_proxy = CreateBinderProxy();
  EXPECT_FALSE(RegisterService(kServiceName,
                               std::unique_ptr<BinderProxy>(service_proxy)));

  // After stopping the service, it should be possible to register it again.
  service->set_state(ServiceInterface::State::STOPPED);
  service_proxy = CreateBinderProxy();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));
  EXPECT_EQ(ServiceInterface::State::STARTED, service->GetState());
  EXPECT_EQ(service_proxy, service->GetProxy());
}

TEST_F(RegistrarTest, QuerySomaForServices) {
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kService1Name("org.example.container.service1");
  const std::string kService2Name("org.example.container.service2");
  ContainerStub* container = AddContainer(kContainerName, kService1Name);
  ServiceStub* service1 = container->AddService(kService1Name);
  ServiceStub* service2 = container->AddService(kService2Name);

  // When a client requests the first service, check that the container is
  // launched and that the client is added to the service (so it can be notified
  // after the service is registered).
  BinderProxy* client1_proxy = CreateBinderProxy();
  EXPECT_TRUE(RequestService(kService1Name,
                             std::unique_ptr<BinderProxy>(client1_proxy)));
  EXPECT_EQ(1, container->launch_count());
  ClientStub* client1 = factory_->GetClient(*client1_proxy);
  ASSERT_TRUE(client1);
  EXPECT_TRUE(service1->HasClient(client1));
  EXPECT_FALSE(service2->HasClient(client1));

  // Check that a second client is also added to the first service.
  BinderProxy* client2_proxy = CreateBinderProxy();
  EXPECT_TRUE(RequestService(kService1Name,
                             std::unique_ptr<BinderProxy>(client2_proxy)));
  EXPECT_EQ(1, container->launch_count());
  ClientStub* client2 = factory_->GetClient(*client2_proxy);
  ASSERT_TRUE(client2);
  EXPECT_TRUE(service1->HasClient(client2));
  EXPECT_FALSE(service2->HasClient(client2));

  // Now make a third client request the second service.
  BinderProxy* client3_proxy = CreateBinderProxy();
  EXPECT_TRUE(RequestService(kService2Name,
                             std::unique_ptr<BinderProxy>(client3_proxy)));
  EXPECT_EQ(1, container->launch_count());
  ClientStub* client3 = factory_->GetClient(*client3_proxy);
  ASSERT_TRUE(client3);
  EXPECT_FALSE(service1->HasClient(client3));
  EXPECT_TRUE(service2->HasClient(client3));
}

// Tests that failure is reported when a ContainerSpec is returned in response
// to a request for a service that it doesn't actually provide.
TEST_F(RegistrarTest, UnknownService) {
  // Create a ContainerSpec that'll get returned for a given service, but don't
  // make it claim to provide that service.
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kServiceName("org.example.container.service");
  AddContainer(kContainerName, kServiceName);

  // A request for the service should fail.
  EXPECT_FALSE(
      RequestService(kServiceName,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));
  // TODO(derat): Once germd communication is present, check that no request was
  // made to launch the container. We can't check the ContainerStub since it
  // ought to have been deleted by this point.
}

// Tests that a second ContainerSpec claiming to provide a service that's
// already provided by an earlier ContainerSpec is ignored.
TEST_F(RegistrarTest, DuplicateService) {
  const std::string kContainer1Name("/foo/org.example.container1.json");
  const std::string kService1Name("org.example.container1.service");
  ContainerStub* container1 = AddContainer(kContainer1Name, kService1Name);
  container1->AddService(kService1Name);

  // Create a second spec, returned for a second service, that also claims
  // ownership of the first container's service.
  const std::string kContainer2Name("/foo/org.example.container2.json");
  const std::string kService2Name("org.example.container2.service");
  ContainerStub* container2 = AddContainer(kContainer2Name, kService2Name);
  container2->AddService(kService1Name);
  container2->AddService(kService2Name);

  // Requesting the first service should succeed, but requesting the second
  // service should fail due to the second container claiming that it also
  // provides the first service.
  EXPECT_TRUE(
      RequestService(kService1Name,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));
  EXPECT_FALSE(
      RequestService(kService2Name,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));
}

// Tests that a duplicate ContainerSpec (i.e. one that was previously received
// from somad, but that now claims to provide a service that it didn't provide
// earlier) gets ignored.
TEST_F(RegistrarTest, ServiceListChanged) {
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kService1Name("org.example.container.service1");
  ContainerStub* container1 = AddContainer(kContainerName, kService1Name);
  container1->AddService(kService1Name);
  EXPECT_TRUE(
      RequestService(kService1Name,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));

  // A request for a second service that returns the already-created spec (which
  // didn't previously claim to provide the second service) should fail.
  const std::string kService2Name("org.example.container.service2");
  ContainerStub* container2 = AddContainer(kContainerName, kService2Name);
  container2->AddService(kService1Name);
  container2->AddService(kService2Name);
  EXPECT_FALSE(
      RequestService(kService2Name,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));
}

// Tests that Registrar doesn't hand out its connection to somad.
TEST_F(RegistrarTest, DontProvideSomaService) {
  EXPECT_FALSE(
      RequestService(soma::kSomaServiceName,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));
}

// Tests various failures when communicating with somad.
TEST_F(RegistrarTest, SomaFailures) {
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kServiceName("org.example.container.service");
  ContainerStub* container = AddContainer(kContainerName, kServiceName);
  container->AddService(kServiceName);

  // Failure should be reported for RPC errors.
  soma_->set_return_value(-1);
  EXPECT_FALSE(
      RequestService(kServiceName,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));

  // Now report that the somad binder proxy died.
  soma_->set_return_value(0);
  binder_manager_->ReportBinderDeath(soma_proxy_);
  EXPECT_FALSE(
      RequestService(kServiceName,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));

  // Register a new proxy for somad and check that the next service request is
  // successful.
  EXPECT_TRUE(InitSoma());
  container = AddContainer(kContainerName, kServiceName);
  container->AddService(kServiceName);
  EXPECT_TRUE(
      RequestService(kServiceName,
                     std::unique_ptr<BinderProxy>(CreateBinderProxy())));
}

}  // namespace
}  // namespace psyche
