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
#include <soma/constants.h>

#include "psyche/common/binder_test_base.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"
#include "psyche/proto_bindings/soma.pb.h"
#include "psyche/proto_bindings/soma.pb.rpc.h"
#include "psyche/proto_bindings/soma_container_spec.pb.h"
#include "psyche/psyched/cell_stub.h"
#include "psyche/psyched/client_stub.h"
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
  Status GetContainerSpec(soma::GetContainerSpecRequest* in,
                          soma::GetContainerSpecResponse* out) override {
    const auto& it = service_specs_.find(in->service_name());
    if (it != service_specs_.end())
      out->mutable_container_spec()->CopyFrom(it->second);
    return return_value_
               ? STATUS_APP_ERROR(return_value_, "GetContainerSpec Error")
               : STATUS_OK();
  }

  Status GetPersistentContainerSpecs(
      soma::GetPersistentContainerSpecsRequest* in,
      soma::GetPersistentContainerSpecsResponse* out) override {
    for (const auto& spec : persistent_specs_)
      out->add_container_specs()->CopyFrom(spec);
    return return_value_ ? STATUS_APP_ERROR(return_value_,
                                            "GetPersistentContainerSpecs Error")
                         : STATUS_OK();
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
        soma_handle_(0),
        // Create an interface immediately so that tests can add persistent
        // cells to it before calling Init().
        soma_(new SomaInterfaceStub) {
    registrar_->SetFactoryForTesting(
        std::unique_ptr<FactoryInterface>(factory_));
  }
  ~RegistrarTest() override {
    // Disgusting hack: manually delete the interface if Init() never got called
    // to transfer ownership to |binder_manager_|.
    // TODO(derat): Remove this after http://brbug.com/648 is fixed.
    if (soma_ && !soma_handle_)
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

  // Initializes |soma_handle_| and |soma_| and registers them with |registrar_|
  // and |binder_manager_|. May be called from within a test to simulate somad
  // restarting and reregistering itself with psyched.
  bool InitSoma(
      std::unique_ptr<SomaInterfaceStub> interface) WARN_UNUSED_RESULT {
    soma_handle_ = CreateBinderProxyHandle();
    soma_ = interface.get();
    binder_manager_->SetTestInterface(
        soma_handle_, std::unique_ptr<IInterface>(interface.release()));
    return RegisterService(soma::kSomaServiceName, soma_handle_);
  }

  // Returns the client that |factory_| created for |client_proxy_handle|,
  // crashing if it doesn't exist.
  ClientStub* GetClientOrDie(uint32_t client_proxy_handle) {
    ClientStub* client = factory_->GetClient(client_proxy_handle);
    CHECK(client) << "No client for proxy " << client_proxy_handle;
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
      uint32_t service_proxy_handle) WARN_UNUSED_RESULT {
    RegisterServiceRequest request;
    request.set_name(service_name);
    BinderProxy(service_proxy_handle).CopyToProtocolBuffer(
        request.mutable_binder());

    RegisterServiceResponse response;
    Status status = registrar_->RegisterService(&request, &response);
    return status.IsOk();
  }

  // Calls |registrar_|'s RequestService method, returning true if a failure
  // wasn't immediately reported back to the client.
  bool RequestService(const std::string& service_name,
                      uint32_t client_proxy_handle) {
    ClientStub* client = factory_->GetClient(client_proxy_handle);
    const int initial_failures =
        client ? client->GetServiceRequestFailures(service_name) : 0;

    RequestServiceRequest request;
    request.set_name(service_name);
    BinderProxy(client_proxy_handle).CopyToProtocolBuffer(
        request.mutable_client_binder());

    Status status = registrar_->RequestService(&request);
    CHECK(status.IsOk()) << "RequestService call for " << service_name
                         << " failed";

    const int new_failures = GetClientOrDie(client_proxy_handle)
                                 ->GetServiceRequestFailures(service_name);
    CHECK_GE(new_failures, initial_failures)
        << "Client " << client_proxy_handle << "'s request failures "
        << "for \"" << service_name << " decreased from " << initial_failures
        << " to " << new_failures;
    return new_failures == initial_failures;
  }

  // Creates a CellStub object named |cell_name| and registers it in
  // |soma_| and |factory_| so it'll be returned for a request for
  // |service_name|. The caller is responsible for calling the stub's
  // AddService() method to make it claim to provide services.
  //
  // The returned object is owned by |registrar_| (and may not
  // persist beyond the request if |registrar_| decides not to keep it).
  CellStub* AddEphemeralCell(const std::string& cell_name,
                             const std::string& service_name) {
    ContainerSpec spec;
    spec.set_name(cell_name);
    soma_->AddEphemeralContainerSpec(spec, service_name);
    CellStub* cell = new CellStub(cell_name);
    factory_->SetCell(cell_name, std::unique_ptr<CellStub>(cell));
    return cell;
  }

  // Creates a CellStub object named |cell_name| and registers it in |soma_| and
  // |factory_| so it'll be returned as a persistent cell.
  //
  // The returned object is owned by |registrar_| (and may not
  // persist beyond the request if |registrar_| decides not to keep it).
  CellStub* AddPersistentCell(const std::string& cell_name) {
    ContainerSpec spec;
    spec.set_name(cell_name);
    spec.set_is_persistent(true);
    soma_->AddPersistentContainerSpec(spec);
    CellStub* cell = new CellStub(cell_name);
    factory_->SetCell(cell_name, std::unique_ptr<CellStub>(cell));
    return cell;
  }

  StubFactory* factory_;  // Owned by |registrar_|.
  std::unique_ptr<Registrar> registrar_;

  uint32_t soma_handle_;
  SomaInterfaceStub* soma_;  // Owned by |binder_manager_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(RegistrarTest);
};

TEST_F(RegistrarTest, RegisterAndRequestService) {
  Init();

  // Register a service.
  const std::string kServiceName("service");
  uint32_t service_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RegisterService(kServiceName, service_handle));

  // Request the service.
  uint32_t client_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RequestService(kServiceName, client_handle));

  // Check that the service was added to the client.
  ClientStub* client = GetClientOrDie(client_handle);
  ServiceStub* service = GetServiceOrDie(kServiceName);
  ASSERT_EQ(1U, client->GetServices().count(service));
}

TEST_F(RegistrarTest, ReregisterService) {
  Init();

  // Register a service.
  const std::string kServiceName("service");
  uint32_t service_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RegisterService(kServiceName, service_handle));

  // The service should hold the correct proxy.
  ServiceStub* service = GetServiceOrDie(kServiceName);
  ASSERT_TRUE(service->GetProxy());
  EXPECT_EQ(service_handle, service->GetProxy()->handle());

  // Trying to register the same service again while it's still running should
  // fail.
  service_handle = CreateBinderProxyHandle();
  EXPECT_FALSE(RegisterService(kServiceName, service_handle));

  // After clearing the service's proxy, it should be possible to register the
  // service again.
  service->SetProxyForTesting(std::unique_ptr<BinderProxy>());
  service_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RegisterService(kServiceName, service_handle));
  ASSERT_TRUE(service->GetProxy());
  EXPECT_EQ(service_handle, service->GetProxy()->handle());
}

TEST_F(RegistrarTest, QuerySomaForServices) {
  Init();
  const std::string kCellName("/foo/org.example.cell.json");
  const std::string kService1Name("org.example.cell.service1");
  const std::string kService2Name("org.example.cell.service2");
  CellStub* cell = AddEphemeralCell(kCellName, kService1Name);
  ServiceStub* service1 = cell->AddService(kService1Name);
  ServiceStub* service2 = cell->AddService(kService2Name);

  // When a client requests the first service, check that the cell is launched
  // and that the client is added to the service (so it can be notified after
  // the service is registered).
  uint32_t client1_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RequestService(kService1Name, client1_handle));
  EXPECT_EQ(1, cell->launch_count());
  ClientStub* client1 = GetClientOrDie(client1_handle);
  EXPECT_TRUE(service1->HasClient(client1));
  EXPECT_FALSE(service2->HasClient(client1));

  // Check that a second client is also added to the first service.
  uint32_t client2_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RequestService(kService1Name, client2_handle));
  EXPECT_EQ(1, cell->launch_count());
  ClientStub* client2 = GetClientOrDie(client2_handle);
  EXPECT_TRUE(service1->HasClient(client2));
  EXPECT_FALSE(service2->HasClient(client2));

  // Now make a third client request the second service.
  uint32_t client3_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RequestService(kService2Name, client3_handle));
  EXPECT_EQ(1, cell->launch_count());
  ClientStub* client3 = GetClientOrDie(client3_handle);
  EXPECT_FALSE(service1->HasClient(client3));
  EXPECT_TRUE(service2->HasClient(client3));
}

// Tests that failure is reported when a ContainerSpec is returned in response
// to a request for a service that it doesn't actually provide.
TEST_F(RegistrarTest, UnknownService) {
  Init();

  // Create a ContainerSpec that'll get returned for a given service, but don't
  // make it claim to provide that service.
  const std::string kCellName("/foo/org.example.cell.json");
  const std::string kServiceName("org.example.cell.service");
  AddEphemeralCell(kCellName, kServiceName);

  // A request for the service should fail.
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxyHandle()));
  // TODO(derat): Once germd communication is present, check that no request was
  // made to launch the cell. We can't check the CellStub since it ought to have
  // been deleted by this point.
}

// Tests that failure is when the service requested has already timed out before
// registering, say, from a previous reqeust.
TEST_F(RegistrarTest, TimedOutService) {
  Init();

  // Register a service.
  const std::string kServiceName("service");
  uint32_t service_handle = CreateBinderProxyHandle();
  EXPECT_TRUE(RegisterService(kServiceName, service_handle));

  // Mark the service as unavailable, which could have happened from a previous
  // request.
  ServiceStub* service = GetServiceOrDie(kServiceName);
  service->SetProxyForTesting(nullptr);
  service->OnServiceUnavailable();

  // A request for the service should fail.
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxyHandle()));
}

// Tests that a second ContainerSpec claiming to provide a service that's
// already provided by an earlier ContainerSpec is ignored.
TEST_F(RegistrarTest, DuplicateService) {
  Init();
  const std::string kCell1Name("/foo/org.example.cell1.json");
  const std::string kService1Name("org.example.cell1.service");
  CellStub* cell1 = AddEphemeralCell(kCell1Name, kService1Name);
  cell1->AddService(kService1Name);

  // Create a second spec, returned for a second service, that also claims
  // ownership of the first cell's service.
  const std::string kCell2Name("/foo/org.example.cell2.json");
  const std::string kService2Name("org.example.cell2.service");
  CellStub* cell2 = AddEphemeralCell(kCell2Name, kService2Name);
  cell2->AddService(kService1Name);
  cell2->AddService(kService2Name);

  // Requesting the first service should succeed, but requesting the second
  // service should fail due to the second cell claiming that it also provides
  // the first service.
  EXPECT_TRUE(RequestService(kService1Name, CreateBinderProxyHandle()));
  EXPECT_FALSE(RequestService(kService2Name, CreateBinderProxyHandle()));
}

// Tests that a duplicate ContainerSpec (i.e. one that was previously received
// from somad, but that now claims to provide a service that it didn't provide
// earlier) gets ignored.
TEST_F(RegistrarTest, ServiceListChanged) {
  Init();
  const std::string kCellName("/foo/org.example.cell.json");
  const std::string kService1Name("org.example.cell.service1");
  CellStub* cell1 = AddEphemeralCell(kCellName, kService1Name);
  cell1->AddService(kService1Name);
  EXPECT_TRUE(RequestService(kService1Name, CreateBinderProxyHandle()));

  // A request for a second service that returns the already-created spec (which
  // didn't previously claim to provide the second service) should fail.
  const std::string kService2Name("org.example.cell.service2");
  CellStub* cell2 = AddEphemeralCell(kCellName, kService2Name);
  cell2->AddService(kService1Name);
  cell2->AddService(kService2Name);
  EXPECT_FALSE(RequestService(kService2Name, CreateBinderProxyHandle()));
}

// Tests that persistent ContainerSpecs are fetched from soma during
// initialization and launched.
TEST_F(RegistrarTest, PersistentCells) {
  // Create two persistent cells with one service each.
  const std::string kCell1Name("/foo/org.example.cell1.json");
  const std::string kService1Name("org.example.cell1.service");
  CellStub* cell1 = AddPersistentCell(kCell1Name);
  ServiceStub* service1 = cell1->AddService(kService1Name);

  const std::string kCell2Name("/foo/org.example.cell2.json");
  const std::string kService2Name("org.example.cell2.service");
  CellStub* cell2 = AddPersistentCell(kCell2Name);
  ServiceStub* service2 = cell2->AddService(kService2Name);

  // After initialization, both cells should be launched.
  Init();
  EXPECT_EQ(1, cell1->launch_count());
  EXPECT_EQ(1, cell2->launch_count());

  // Their services should also be available to clients.
  EXPECT_TRUE(RequestService(kService1Name, CreateBinderProxyHandle()));
  EXPECT_EQ(1U, service1->num_clients());
  EXPECT_TRUE(RequestService(kService2Name, CreateBinderProxyHandle()));
  EXPECT_EQ(1U, service2->num_clients());
}

// Tests that Registrar doesn't hand out its connection to somad.
TEST_F(RegistrarTest, DontProvideSomaService) {
  Init();
  EXPECT_FALSE(RequestService(soma::kSomaServiceName,
                              CreateBinderProxyHandle()));
}

// Tests various failures when communicating with somad.
TEST_F(RegistrarTest, SomaFailures) {
  Init();
  const std::string kCellName("/foo/org.example.cell.json");
  const std::string kServiceName("org.example.cell.service");
  CellStub* cell = AddEphemeralCell(kCellName, kServiceName);
  cell->AddService(kServiceName);

  // Failure should be reported for RPC errors.
  soma_->set_return_value(-1);
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxyHandle()));

  // Now report that the somad binder proxy died.
  soma_->set_return_value(0);
  binder_manager_->ReportBinderDeath(soma_handle_);
  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxyHandle()));

  // Register a new proxy for somad and check that the next service request is
  // successful.
  EXPECT_TRUE(
      InitSoma(std::unique_ptr<SomaInterfaceStub>(new SomaInterfaceStub)));
  cell = AddEphemeralCell(kCellName, kServiceName);
  cell->AddService(kServiceName);
  EXPECT_TRUE(RequestService(kServiceName, CreateBinderProxyHandle()));
}

// Tests that Registrar doesn't hand out its connection to germd.
TEST_F(RegistrarTest, DontProvideGermService) {
  Init();
  EXPECT_FALSE(RequestService(germ::kGermServiceName,
                              CreateBinderProxyHandle()));
}

// Tests that Registrar reports cell launch failures to clients.
TEST_F(RegistrarTest, CellLaunchFailure) {
  Init();

  const std::string kCellName("/foo/org.example.cell.json");
  const std::string kServiceName("org.example.cell.service");
  CellStub* cell = AddEphemeralCell(kCellName, kServiceName);
  cell->AddService(kServiceName);
  cell->set_launch_return_value(false);

  EXPECT_FALSE(RequestService(kServiceName, CreateBinderProxyHandle()));
}

}  // namespace
}  // namespace psyche
