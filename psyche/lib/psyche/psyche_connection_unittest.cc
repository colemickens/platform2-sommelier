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
#include <chromeos/make_unique_ptr.h>
#include <protobinder/binder_manager_stub.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/iinterface.h>

// binder.h requires types.h to be included first.
#include <sys/types.h>
#include <linux/android/binder.h>  // NOLINT(build/include_alpha)

#include "psyche/common/binder_test_base.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

using chromeos::make_unique_ptr;
using protobinder::BinderHost;
using protobinder::BinderProxy;

namespace psyche {
namespace {

// Stub implementation of IPsyched, the binder interface implemented by psyched.
class PsychedInterfaceStub : public IPsyched {
 public:
  PsychedInterfaceStub()
      : app_success_(true),
        ipc_success_(true),
        weak_ptr_factory_(this) {}
  ~PsychedInterfaceStub() override = default;

  void set_app_success(bool success) { app_success_ = success; }
  void set_ipc_success(bool success) { ipc_success_ = success; }

  // Keys are service names; values are binder cookies.
  using HostMap = std::map<std::string, binder_uintptr_t>;
  const HostMap& registered_services() const { return registered_services_; }

  // Sets a proxy handle to be returned in response to a RequestService call for
  // |service_name|.
  void SetService(const std::string& service_name, uint32_t handle) {
    services_to_return_[service_name] = handle;
  }

  // IPsyched:
  Status RegisterService(RegisterServiceRequest* in,
                         RegisterServiceResponse* out) override {
    registered_services_[in->name()] =
        static_cast<binder_uintptr_t>(in->binder().host_cookie());
    if (!ipc_success_)
      return STATUS_BINDER_ERROR(Status::DEAD_ENDPOINT);
    return app_success_
               ? STATUS_OK()
               : STATUS_APP_ERROR(RegisterServiceResponse::INVALID_NAME,
                                  "RegisterService Error");
  }

  Status RequestService(RequestServiceRequest* in) override {
    if (!ipc_success_)
      return STATUS_BINDER_ERROR(Status::DEAD_ENDPOINT);

    const std::string service_name(in->name());
    const auto& it = services_to_return_.find(service_name);
    uint32_t service_handle = it != services_to_return_.end() ? it->second : 0;

    binder_uintptr_t client_cookie = in->client_binder().host_cookie();
    BinderHost* client_binder =
        static_cast<BinderManagerStub*>(BinderManagerInterface::Get())
            ->GetHostForCookie(client_cookie);
    CHECK(client_binder) << "No host registered for cookie " << client_cookie;

    // Post a task to call the client's ReceiveService method asynchronously to
    // simulate what happens in reality, where RequestService and ReceiveService
    // are both one-way binder calls.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PsychedInterfaceStub::CallReceiveService,
                   weak_ptr_factory_.GetWeakPtr(),
                   static_cast<IPsycheClientHostInterface*>(client_binder),
                   service_name, service_handle));
    return STATUS_OK();
  }

 private:
  // Passes |service_name| and |service_proxy| to |client_interface|'s
  // ReceiveService method.
  void CallReceiveService(IPsycheClientHostInterface* client_interface,
                          std::string service_name,
                          uint32_t service_handle) {
    ReceiveServiceRequest request;
    request.set_name(service_name);
    if (service_handle)
      request.mutable_binder()->set_proxy_handle(service_handle);
    CHECK(client_interface->ReceiveService(&request));
  }

  // If set returns an Application Error.
  bool app_success_;

  // Retuns IPC an error if false.
  bool ipc_success_;

  // Services that have been passed to RegisterService().
  HostMap registered_services_;

  // Service proxy handles that should be returned by RequestService(), keyed by
  // service name.
  using ProxyMap = std::map<std::string, uint32_t>;
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
  PsycheConnectionTest() : psyched_(new PsychedInterfaceStub) {
    int32_t psyched_handle = CreateBinderProxyHandle();
    binder_manager_->SetTestInterface(psyched_handle,
                                      std::unique_ptr<IInterface>(psyched_));
    connection_.SetProxyForTesting(
        make_unique_ptr(new BinderProxy(psyched_handle)));
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
  psyched_->set_ipc_success(false);
  EXPECT_FALSE(connection_.RegisterService(kServiceName, &host));

  // Simulate psyched reporting failure.
  psyched_->set_ipc_success(true);
  psyched_->set_app_success(false);
  EXPECT_FALSE(connection_.RegisterService(kServiceName, &host));

  // Simulate success. Verify that the host was passed to psyched.
  psyched_->set_app_success(true);
  EXPECT_TRUE(connection_.RegisterService(kServiceName, &host));
  auto it = psyched_->registered_services().find(kServiceName);
  ASSERT_TRUE(it != psyched_->registered_services().end());
  EXPECT_EQ(host.cookie(), it->second);
}

TEST_F(PsycheConnectionTest, GetService) {
  const std::string kServiceName("org.example.cell.service");

  // Check that GetService() returns false when an RPC error is encountered.
  psyched_->set_ipc_success(false);
  ServiceReceiver rpc_error_receiver;
  EXPECT_FALSE(connection_.GetService(
      kServiceName, base::Bind(&ServiceReceiver::ReceiveService,
                               base::Unretained(&rpc_error_receiver))));
  rpc_error_receiver.RunMessageLoop();
  EXPECT_EQ(0, rpc_error_receiver.num_calls());

  // Now request in an unknown service, which should result in a null proxy
  // being returned.
  psyched_->set_ipc_success(true);
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
  uint32_t service_handle = CreateBinderProxyHandle();
  psyched_->SetService(kServiceName, service_handle);
  ServiceReceiver successful_receiver;
  EXPECT_TRUE(connection_.GetService(
      kServiceName, base::Bind(&ServiceReceiver::ReceiveService,
                               base::Unretained(&successful_receiver))));
  successful_receiver.RunMessageLoop();

  EXPECT_EQ(1, successful_receiver.num_calls());
  ASSERT_TRUE(successful_receiver.proxy());
  EXPECT_EQ(service_handle, successful_receiver.proxy()->handle());

  EXPECT_EQ(1, unknown_service_receiver.num_calls());
  ASSERT_TRUE(unknown_service_receiver.proxy());
  EXPECT_EQ(service_handle, unknown_service_receiver.proxy()->handle());
}

}  // namespace
}  // namespace psyche
