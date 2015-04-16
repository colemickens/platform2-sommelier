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
#include <germ/constants.h>
#include <gtest/gtest.h>
#include <protobinder/binder_manager_stub.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iinterface.h>
#include <protobinder/proto_util.h>
#include <soma/constants.h>

#include "psyche/common/binder_test_base.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/proto_bindings/soma.pb.h"
#include "psyche/proto_bindings/soma.pb.rpc.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/client_stub.h"
#include "psyche/psyched/container_stub.h"
#include "psyche/psyched/service_stub.h"
#include "psyche/psyched/stub_factory.h"

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
  void AddEphemeralContainerSpec(const ContainerSpec& spec,
                                 const std::string& service_name) {
    service_specs_[service_name] = spec;
  }

  // Adds a ContainerSpec to be returned by GetPersistentContainerSpecs().
  void AddPersistentContainerSpec(const ContainerSpec& spec) {
    persistent_specs_.push_back(spec);
  }

  // ISoma:
  int GetContainerSpec(soma::GetContainerSpecRequest* in,
                       soma::GetContainerSpecResponse* out) override {
    const auto& it = service_specs_.find(in->service_name());
    if (it != service_specs_.end())
      out->mutable_container_spec()->CopyFrom(it->second);
    return return_value_;
  }

  int GetPersistentContainerSpecs(
      soma::GetPersistentContainerSpecsRequest* in,
      soma::GetPersistentContainerSpecsResponse* out) override {
    for (const auto& spec : persistent_specs_)
      out->add_container_specs()->CopyFrom(spec);
    return return_value_;
  }

 private:
  // Keyed by service name for which the spec should be returned.
  std::map<std::string, ContainerSpec> service_specs_;

  std::vector<ContainerSpec> persistent_specs_;

  // binder result returned by handlers.
  int return_value_;

  DISALLOW_COPY_AND_ASSIGN(SomaInterfaceStub);
};

class RegistrarTest : public BinderTestBase {
 public:
  RegistrarTest()
      : factory_(new StubFactory),
        registrar_(new Registrar),
        soma_proxy_(nullptr),
        // Create an interface immediately so that tests can add persistent
        // containers to it before calling Init().
        soma_(new SomaInterfaceStub) {
    registrar_->SetFactoryForTesting(
        std::unique_ptr<FactoryInterface>(factory_));
  }
  ~RegistrarTest() override {
    // Disgusting hack: manually delete the interface if Init() never got called
    // to transfer ownership to |binder_manager_|.
    // TODO(derat): Remove this after http://brbug.com/648 is fixed.
    if (soma_ && !soma_proxy_)
      delete soma_;
  }

 protected:
  // Performs initialization. Should be called at the beginning of each test;
  // separated from the c'tor so that persistent services can be created first.
  void Init() {
    registrar_->Init();
    // Pass ownership of |soma_| to InitSoma(), which will pass it to
    // |binder_manager_|.
    CHECK(InitSoma(std::unique_ptr<SomaInterfaceStub>(soma_)));
  }

  // Initializes |soma_proxy_| and |soma_| and registers them with |registrar_|
  // and |binder_manager_|. May be called from within a test to simulate somad
  // restarting and reregistering itself with psyched.
  bool InitSoma(
      std::unique_ptr<SomaInterfaceStub> interface) WARN_UNUSED_RESULT {
    soma_proxy_ = CreateBinderProxy().release();
    soma_ = interface.get();
    binder_manager_->SetTestInterface(
        soma_proxy_, scoped_ptr<IInterface>(interface.release()));
    return RegisterService(soma::kSomaServiceName,
                           std::unique_ptr<BinderProxy>(soma_proxy_));
  }

  // Returns the client that |factory_| created for |client_proxy|, crashing if
  // it doesn't exist.
  ClientStub* GetClientOrDie(const BinderProxy& client_proxy) {
    ClientStub* client = factory_->GetClient(client_proxy);
    CHECK(client) << "No client for proxy " << client_proxy.handle();
    return client;
  }

  // Returns the service that |factory_| created for |service_name|, crashing if
  // it doesn't exist.
  ServiceStub* GetServiceOrDie(const std::string& service_name) {
    ServiceStub* service = factory_->GetService(service_name);
    CHECK(service) << "No service named \"" << service_name << "\"";
    return service;
  }

  // Calls |registrar_|'s RegisterService method, returning true on success.
  bool RegisterService(
      const std::string& service_name,
      std::unique_ptr<BinderProxy> service_proxy) WARN_UNUSED_RESULT {
    RegisterServiceRequest request;
    request.set_name(service_name);
    // |service_proxy| will be extracted from the protocol buffer into another
    // std::unique_ptr, so release it from the original one here.
    protobinder::StoreBinderInProto(*(service_proxy.release()),
                                    request.mutable_binder());

    RegisterServiceResponse response;
    CHECK_EQ(registrar_->RegisterService(&request, &response), 0)
        << "RegisterService call for " << service_name << " failed";
    return response.success();
  }

  // Calls |registrar_|'s RequestService method, returning true if a failure
  // wasn't immediately reported back to the client.
  bool RequestService(const std::string& service_name,
                      std::unique_ptr<BinderProxy> client_proxy) {
    // Hang on to the proxy so we can use it to find the client later.
    BinderProxy* client_proxy_ptr = client_proxy.get();
    ClientStub* client = factory_->GetClient(*client_proxy_ptr);
    const int initial_failures =
        client ? client->GetServiceRequestFailures(service_name) : 0;

    RequestServiceRequest request;
    request.set_name(service_name);
    // |service_proxy| will be extracted from the protocol buffer into another
    // std::unique_ptr, so release it from the original one here.
    protobinder::StoreBinderInProto(*(client_proxy.release()),
                                    request.mutable_client_binder());

    CHECK_EQ(registrar_->RequestService(&request), 0)
        << "RequestService call for " << service_name << " failed";

    const int new_failures = GetClientOrDie(*client_proxy_ptr)
                                 ->GetServiceRequestFailures(service_name);
    CHECK_GE(new_failures, initial_failures)
        << "Client " << client_proxy_ptr->handle() << "'s request failures "
        << "for \"" << service_name << " decreased from " << initial_failures
        << " to " << new_failures;
    return new_failures == initial_failures;
  }

  // Creates a ContainerStub object named |container_name| and registers it in
  // |soma_| and |factory_| so it'll be returned for a request for
  // |service_name|. The caller is responsible for calling the stub's
  // AddService() method to make it claim to provide services.
  //
  // The returned object is owned by |registrar_| (and may not
  // persist beyond the request if |registrar_| decides not to keep it).
  ContainerStub* AddEphemeralContainer(const std::string& container_name,
                                       const std::string& service_name) {
    ContainerSpec spec;
    spec.set_name(container_name);
    soma_->AddEphemeralContainerSpec(spec, service_name);
    ContainerStub* container = new ContainerStub(container_name);
    factory_->SetContainer(container_name,
                           std::unique_ptr<ContainerStub>(container));
    return container;
  }

  // Creates a ContainerStub object named |container_name| and registers it in
  // |soma_| and |factory_| so it'll be returned as a persistent container.
  //
  // The returned object is owned by |registrar_| (and may not
  // persist beyond the request if |registrar_| decides not to keep it).
  ContainerStub* AddPersistentContainer(const std::string& container_name) {
    ContainerSpec spec;
    spec.set_name(container_name);
    spec.set_is_persistent(true);
    soma_->AddPersistentContainerSpec(spec);
    ContainerStub* container = new ContainerStub(container_name);
    factory_->SetContainer(container_name,
                           std::unique_ptr<ContainerStub>(container));
    return container;
  }

  StubFactory* factory_;  // Owned by |registrar_|.
  std::unique_ptr<Registrar> registrar_;

  BinderProxy* soma_proxy_;  // Owned by |registrar_|.
  SomaInterfaceStub* soma_;  // Owned by |binder_manager_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistrarTest);
};

TEST_F(RegistrarTest, RegisterAndRequestService) {
  Init();

  // Register a service.
  const std::string kServiceName("service");
  BinderProxy* service_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));

  // Request the service.
  BinderProxy* client_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RequestService(kServiceName,
                             std::unique_ptr<BinderProxy>(client_proxy)));

  // Check that the service was added to the client.
  ClientStub* client = GetClientOrDie(*client_proxy);
  ServiceStub* service = GetServiceOrDie(kServiceName);
  ASSERT_EQ(1U, client->GetServices().count(service));
}

TEST_F(RegistrarTest, ReregisterService) {
  Init();

  // Register a service.
  const std::string kServiceName("service");
  BinderProxy* service_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));

  // The service should hold the correct proxy.
  ServiceStub* service = GetServiceOrDie(kServiceName);
  EXPECT_EQ(service_proxy, service->GetProxy());

  // Trying to register the same service again while it's still running should
  // fail.
  service_proxy = CreateBinderProxy().release();
  EXPECT_FALSE(RegisterService(kServiceName,
                               std::unique_ptr<BinderProxy>(service_proxy)));

  // After clearing the service's proxy, it should be possible to register the
  // service again.
  service->SetProxyForTesting(std::unique_ptr<BinderProxy>());
  service_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RegisterService(kServiceName,
                              std::unique_ptr<BinderProxy>(service_proxy)));
  EXPECT_EQ(service_proxy, service->GetProxy());
}

TEST_F(RegistrarTest, QuerySomaForServices) {
  Init();
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kService1Name("org.example.container.service1");
  const std::string kService2Name("org.example.container.service2");
  ContainerStub* container =
      AddEphemeralContainer(kContainerName, kService1Name);
  ServiceStub* service1 = container->AddService(kService1Name);
  ServiceStub* service2 = container->AddService(kService2Name);

  // When a client requests the first service, check that the container is
  // launched and that the client is added to the service (so it can be notified
  // after the service is registered).
  BinderProxy* client1_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RequestService(kService1Name,
                             std::unique_ptr<BinderProxy>(client1_proxy)));
  EXPECT_EQ(1, container->launch_count());
  ClientStub* client1 = GetClientOrDie(*client1_proxy);
  EXPECT_TRUE(service1->HasClient(client1));
  EXPECT_FALSE(service2->HasClient(client1));

  // Check that a second client is also added to the first service.
  BinderProxy* client2_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RequestService(kService1Name,
                             std::unique_ptr<BinderProxy>(client2_proxy)));
  EXPECT_EQ(1, container->launch_count());
  ClientStub* client2 = GetClientOrDie(*client2_proxy);
  EXPECT_TRUE(service1->HasClient(client2));
  EXPECT_FALSE(service2->HasClient(client2));

  // Now make a third client request the second service.
  BinderProxy* client3_proxy = CreateBinderProxy().release();
  EXPECT_TRUE(RequestService(kService2Name,
                             std::unique_ptr<BinderProxy>(client3_proxy)));
  EXPECT_EQ(1, container->launch_count());
  ClientStub* client3 = GetClientOrDie(*client3_proxy);
  EXPECT_FALSE(service1->HasClient(client3));
  EXPECT_TRUE(service2->HasClient(client3));
}

// Tests that failure is reported when a ContainerSpec is returned in response
// to a request for a service that it doesn't actually provide.
TEST_F(RegistrarTest, UnknownService) {
  Init();

  // Create a ContainerSpec that'll get returned for a given service, but don't
  // make it claim to provide that service.
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kServiceName("org.example.container.service");
  AddEphemeralContainer(kContainerName, kServiceName);

  // A request for the service should fail.
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxy()));
  // TODO(derat): Once germd communication is present, check that no request was
  // made to launch the container. We can't check the ContainerStub since it
  // ought to have been deleted by this point.
}

// Tests that a second ContainerSpec claiming to provide a service that's
// already provided by an earlier ContainerSpec is ignored.
TEST_F(RegistrarTest, DuplicateService) {
  Init();
  const std::string kContainer1Name("/foo/org.example.container1.json");
  const std::string kService1Name("org.example.container1.service");
  ContainerStub* container1 =
      AddEphemeralContainer(kContainer1Name, kService1Name);
  container1->AddService(kService1Name);

  // Create a second spec, returned for a second service, that also claims
  // ownership of the first container's service.
  const std::string kContainer2Name("/foo/org.example.container2.json");
  const std::string kService2Name("org.example.container2.service");
  ContainerStub* container2 =
      AddEphemeralContainer(kContainer2Name, kService2Name);
  container2->AddService(kService1Name);
  container2->AddService(kService2Name);

  // Requesting the first service should succeed, but requesting the second
  // service should fail due to the second container claiming that it also
  // provides the first service.
  EXPECT_TRUE(RequestService(kService1Name, CreateBinderProxy()));
  EXPECT_FALSE(RequestService(kService2Name, CreateBinderProxy()));
}

// Tests that a duplicate ContainerSpec (i.e. one that was previously received
// from somad, but that now claims to provide a service that it didn't provide
// earlier) gets ignored.
TEST_F(RegistrarTest, ServiceListChanged) {
  Init();
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kService1Name("org.example.container.service1");
  ContainerStub* container1 =
      AddEphemeralContainer(kContainerName, kService1Name);
  container1->AddService(kService1Name);
  EXPECT_TRUE(RequestService(kService1Name, CreateBinderProxy()));

  // A request for a second service that returns the already-created spec (which
  // didn't previously claim to provide the second service) should fail.
  const std::string kService2Name("org.example.container.service2");
  ContainerStub* container2 =
      AddEphemeralContainer(kContainerName, kService2Name);
  container2->AddService(kService1Name);
  container2->AddService(kService2Name);
  EXPECT_FALSE(RequestService(kService2Name, CreateBinderProxy()));
}

// Tests that persistent ContainerSpecs are fetched from soma during
// initialization and launched.
TEST_F(RegistrarTest, PersistentContainers) {
  // Create two persistent containers with one service each.
  const std::string kContainer1Name("/foo/org.example.container1.json");
  const std::string kService1Name("org.example.container1.service");
  ContainerStub* container1 = AddPersistentContainer(kContainer1Name);
  ServiceStub* service1 = container1->AddService(kService1Name);

  const std::string kContainer2Name("/foo/org.example.container2.json");
  const std::string kService2Name("org.example.container2.service");
  ContainerStub* container2 = AddPersistentContainer(kContainer2Name);
  ServiceStub* service2 = container2->AddService(kService2Name);

  // After initialization, both containers should be launched.
  Init();
  EXPECT_EQ(1, container1->launch_count());
  EXPECT_EQ(1, container2->launch_count());

  // Their services should also be available to clients.
  EXPECT_TRUE(RequestService(kService1Name, CreateBinderProxy()));
  EXPECT_EQ(1U, service1->num_clients());
  EXPECT_TRUE(RequestService(kService2Name, CreateBinderProxy()));
  EXPECT_EQ(1U, service2->num_clients());
}

// Tests that Registrar doesn't hand out its connection to somad.
TEST_F(RegistrarTest, DontProvideSomaService) {
  Init();
  EXPECT_FALSE(RequestService(soma::kSomaServiceName, CreateBinderProxy()));
}

// Tests various failures when communicating with somad.
TEST_F(RegistrarTest, SomaFailures) {
  Init();
  const std::string kContainerName("/foo/org.example.container.json");
  const std::string kServiceName("org.example.container.service");
  ContainerStub* container =
      AddEphemeralContainer(kContainerName, kServiceName);
  container->AddService(kServiceName);

  // Failure should be reported for RPC errors.
  soma_->set_return_value(-1);
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxy()));

  // Now report that the somad binder proxy died.
  soma_->set_return_value(0);
  binder_manager_->ReportBinderDeath(soma_proxy_);
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxy()));

  // Register a new proxy for somad and check that the next service request is
  // successful.
  EXPECT_TRUE(
      InitSoma(std::unique_ptr<SomaInterfaceStub>(new SomaInterfaceStub)));
  container = AddEphemeralContainer(kContainerName, kServiceName);
  container->AddService(kServiceName);
  EXPECT_TRUE(RequestService(kServiceName, CreateBinderProxy()));
}

// Tests that Registrar doesn't hand out its connection to germd.
TEST_F(RegistrarTest, DontProvideGermService) {
  Init();
  EXPECT_FALSE(RequestService(germ::kGermServiceName, CreateBinderProxy()));
}

// TODO(mcolagrosso): Add tests for failures to communicate to germd, similar to
// SomaFailures above.

}  // namespace
}  // namespace psyche
