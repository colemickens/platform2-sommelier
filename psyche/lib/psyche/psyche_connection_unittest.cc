// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/lib/psyche/psyche_connection.h"

#include <map>
#include <memory>
#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <protobinder/binder_manager_stub.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iinterface.h>

#include "psyche/common/binder_test_base.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

using protobinder::BinderHost;
using protobinder::BinderProxy;

namespace psyche {
namespace {

// Stub implementation of IPsyched, the binder interface implemented by psyched.
class PsychedInterfaceStub : public IPsyched {
 public:
  PsychedInterfaceStub()
      : return_value_(0),
        report_success_(true),
        weak_ptr_factory_(this) {}
  ~PsychedInterfaceStub() override = default;

  void set_return_value(int value) { return_value_ = value; }
  void set_report_success(bool success) { report_success_ = success; }

  // Keyed by service name.
  using HostMap = std::map<std::string, BinderHost*>;
  const HostMap& registered_services() const { return registered_services_; }

  // Sets a proxy to be returned in response to a RequestService call for
  // |service_name|.
  void SetService(const std::string& service_name,
                  std::unique_ptr<BinderProxy> proxy) {
    services_to_return_[service_name] = std::move(proxy);
  }

  // IPsyched:
  int RegisterService(RegisterServiceRequest* in,
                      RegisterServiceResponse* out) override {
    // So gross. Usually libprotobinder creates a BinderProxy object
    // automatically in response to an incoming call and copies its address into
    // the protobuf. We're not crossing a process boundary (or even going
    // through libprotobinder, for that matter), so the underlying object here
    // is just what we put into it: the address of the original BinderHost.
    BinderHost* host = reinterpret_cast<BinderHost*>(in->binder().ibinder());
    registered_services_[in->name()] = host;
    out->set_success(report_success_);
    return return_value_;
  }

  int RequestService(RequestServiceRequest* in) override {
    if (return_value_ != 0)
      return return_value_;

    const std::string service_name(in->name());
    std::unique_ptr<BinderProxy> service_proxy;
    const auto& it = services_to_return_.find(service_name);
    if (it != services_to_return_.end()) {
      service_proxy = std::move(it->second);
      services_to_return_.erase(it);
    }

    // This is PsycheConnection's client interface. We're not crossing a process
    // boundary here, so we don't take ownership of this.
    CHECK(in->client_binder().ibinder());
    IBinder* client_binder =
        reinterpret_cast<IBinder*>(in->client_binder().ibinder());

    // Post a task to call the client's ReceiveService method asynchronously to
    // simulate what happens in reality, where RequestService and ReceiveService
    // are both one-way binder calls.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PsychedInterfaceStub::CallReceiveService,
                   weak_ptr_factory_.GetWeakPtr(),
                   static_cast<IPsycheClientHostInterface*>(client_binder),
                   service_name, base::Passed(&service_proxy)));
    return 0;
  }

 private:
  // Passes |service_name| and |service_proxy| to |client_interface|'s
  // ReceiveService method.
  void CallReceiveService(IPsycheClientHostInterface* client_interface,
                          std::string service_name,
                          std::unique_ptr<BinderProxy> service_proxy) {
    ReceiveServiceRequest request;
    request.set_name(service_name);
    if (service_proxy) {
      // libprotobinder would usually construct its own BinderProxy when it
      // reads the binder handle from the protocol buffer, so pass ownership
      // back to PsycheConnection here.
      request.mutable_binder()->set_ibinder(
          reinterpret_cast<uint64_t>(service_proxy.release()));
    }
    CHECK_EQ(client_interface->ReceiveService(&request), 0);
  }

  // Value to return for RPCs.
  int return_value_;

  // Value to write to |success| fields in responses.
  bool report_success_;

  // Services that have been passed to RegisterService().
  HostMap registered_services_;

  // Services that should be returned by RequestService(), keyed by service
  // name.
  using ProxyMap = std::map<std::string, std::unique_ptr<BinderProxy>>;
  ProxyMap services_to_return_;

  // Keep this member last.
  base::WeakPtrFactory<PsychedInterfaceStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PsychedInterfaceStub);
};

// Arbitrary BinderHost implementation that can be passed to psyched.
class FakeHost : public BinderHost {
 public:
  FakeHost() = default;
  ~FakeHost() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeHost);
};

// Class that records a proxy that's passed to it.
class ServiceReceiver {
 public:
  ServiceReceiver() : num_calls_(0) {}
  ~ServiceReceiver() = default;

  int num_calls() const { return num_calls_; }
  const BinderProxy* proxy() const { return proxy_.get(); }

  // Resets internal state.
  void Reset() {
    num_calls_ = 0;
    proxy_.reset();
  }

  // Saves |proxy| to |proxy_| and stops the message loop if |quit_closure_| is
  // set. Intended to be passed as a callback to PsycheConnection::GetService().
  void ReceiveService(std::unique_ptr<BinderProxy> proxy) {
    num_calls_++;
    proxy_ = std::move(proxy);
    if (!quit_closure_.is_null()) {
      quit_closure_.Run();
      quit_closure_.Reset();
    }
  }

  // Runs the message loop until ReceiveService() is called or the loop is idle.
  void RunMessageLoop() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.RunUntilIdle();
  }

 private:
  // Number of times that ReceiveService() has been called.
  int num_calls_;

  // Last proxy that was passed to ReceiveService().
  std::unique_ptr<BinderProxy> proxy_;

  // Callback for stopping the message loop.
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ServiceReceiver);
};

class PsycheConnectionTest : public BinderTestBase {
 public:
  PsycheConnectionTest() : psyched_(nullptr) {
    std::unique_ptr<BinderProxy> proxy = CreateBinderProxy();
    psyched_ = new PsychedInterfaceStub;
    binder_manager_->SetTestInterface(proxy.get(),
                                      std::unique_ptr<IInterface>(psyched_));
    connection_.SetProxyForTesting(std::move(proxy));
    CHECK(connection_.Init());
  }
  ~PsycheConnectionTest() override = default;

 protected:
  PsycheConnection connection_;
  PsychedInterfaceStub* psyched_;  // Owned by |binder_manager_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(PsycheConnectionTest);
};

TEST_F(PsycheConnectionTest, RegisterService) {
  const std::string kServiceName("org.example.cell.service");
  FakeHost host;

  // Simulate an RPC error.
  psyched_->set_return_value(-1);
  EXPECT_FALSE(connection_.RegisterService(kServiceName, &host));

  // Simulate psyched reporting failure.
  psyched_->set_return_value(0);
  psyched_->set_report_success(false);
  EXPECT_FALSE(connection_.RegisterService(kServiceName, &host));

  // Simulate success. Verify that the host was passed to psyched.
  psyched_->set_report_success(true);
  EXPECT_TRUE(connection_.RegisterService(kServiceName, &host));
  auto it = psyched_->registered_services().find(kServiceName);
  ASSERT_TRUE(it != psyched_->registered_services().end());
  EXPECT_EQ(&host, it->second);
}

TEST_F(PsycheConnectionTest, GetService) {
  const std::string kServiceName("org.example.cell.service");

  // Check that GetService() returns false when an RPC error is encountered.
  psyched_->set_return_value(-1);
  ServiceReceiver rpc_error_receiver;
  EXPECT_FALSE(connection_.GetService(
      kServiceName, base::Bind(&ServiceReceiver::ReceiveService,
                               base::Unretained(&rpc_error_receiver))));
  rpc_error_receiver.RunMessageLoop();
  EXPECT_EQ(0, rpc_error_receiver.num_calls());

  // Now request in an unknown service, which should result in a null proxy
  // being returned.
  psyched_->set_return_value(0);
  ServiceReceiver unknown_service_receiver;
  EXPECT_TRUE(connection_.GetService(
      kServiceName, base::Bind(&ServiceReceiver::ReceiveService,
                               base::Unretained(&unknown_service_receiver))));
  unknown_service_receiver.RunMessageLoop();
  EXPECT_EQ(1, unknown_service_receiver.num_calls());
  EXPECT_FALSE(unknown_service_receiver.proxy());
  unknown_service_receiver.Reset();

  // Create the service so a second request will succeed. Both the old and new
  // callbacks should be invoked.
  BinderProxy* service_proxy = CreateBinderProxy().release();
  psyched_->SetService(kServiceName,
                       std::unique_ptr<BinderProxy>(service_proxy));
  ServiceReceiver successful_receiver;
  EXPECT_TRUE(connection_.GetService(
      kServiceName, base::Bind(&ServiceReceiver::ReceiveService,
                               base::Unretained(&successful_receiver))));
  successful_receiver.RunMessageLoop();

  // We won't receive the original proxy (new ones are created so they can be
  // passed to each callback), but the underlying binder handle should match.
  EXPECT_EQ(1, successful_receiver.num_calls());
  ASSERT_TRUE(successful_receiver.proxy());
  EXPECT_EQ(service_proxy->handle(), successful_receiver.proxy()->handle());

  EXPECT_EQ(1, unknown_service_receiver.num_calls());
  ASSERT_TRUE(unknown_service_receiver.proxy());
  EXPECT_EQ(service_proxy->handle(),
            unknown_service_receiver.proxy()->handle());
}

}  // namespace
}  // namespace psyche
