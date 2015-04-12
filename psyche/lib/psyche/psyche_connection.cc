// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/lib/psyche/psyche_connection.h"

#include <map>

#include <base/logging.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/ibinder.h>
#include <protobinder/iservice_manager.h>
#include <protobinder/proto_util.h>
#include <protobinder/protobinder.h>

#include "psyche/common/constants.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

using protobinder::BinderHost;
using protobinder::BinderProxy;

namespace psyche {

// This class's methods mirror PsycheConnection's.
class PsycheConnection::Impl : public IPsycheClientHostInterface {
 public:
  Impl() = default;
  ~Impl() override = default;

  void SetProxyForTesting(std::unique_ptr<BinderProxy> psyched_proxy) {
    CHECK(!psyched_proxy_);
    psyched_proxy_ = std::move(psyched_proxy);
  }

  bool Init() {
    if (!psyched_proxy_) {
      psyched_proxy_.reset(
          static_cast<BinderProxy*>(
              protobinder::GetServiceManager()->GetService(
                  kPsychedServiceManagerName)));
      if (!psyched_proxy_) {
        LOG(ERROR) << "Failed to connect to psyched";
        return false;
      }
    }
    psyched_interface_.reset(
        protobinder::BinderToInterface<IPsyched>(psyched_proxy_.get()));
    return true;
  }

  bool RegisterService(const std::string& service_name, BinderHost* service) {
    DCHECK(psyched_interface_);

    RegisterServiceRequest request;
    request.set_name(service_name);
    protobinder::StoreBinderInProto(*service, request.mutable_binder());

    RegisterServiceResponse response;

    const int result = psyched_interface_->RegisterService(&request, &response);
    if (result != SUCCESS) {
      LOG(ERROR) << "RegisterService binder call failed with " << result;
      return false;
    }
    if (!response.success()) {
      LOG(ERROR) << "psyched reported failure when registering "
                 << service_name;
      return false;
    }
    return true;
  }

  bool GetService(const std::string& service_name,
                  const GetServiceCallback& callback) {
    DCHECK(psyched_interface_);

    RequestServiceRequest request;
    request.set_name(service_name);
    protobinder::StoreBinderInProto(*this, request.mutable_client_binder());
    const int result = psyched_interface_->RequestService(&request);
    if (result != SUCCESS) {
      LOG(ERROR) << "RequestService binder call failed with " << result;
      return false;
    }

    // Register the callback last so it doesn't stick around if the RPC failed.
    // This is safe to do since we're making a one-way call, i.e. psyched's
    // asynchronous ReceiveService call won't be handled until control returns
    // to the message loop.
    get_service_callbacks_[service_name] = callback;
    return true;
  }

  // IPsycheClientHostInterface:
  int ReceiveService(ReceiveServiceRequest* in) override {
    const std::string service_name = in->name();
    std::unique_ptr<BinderProxy> proxy;
    if (in->has_binder())
      proxy = protobinder::ExtractBinderFromProto(in->mutable_binder());

    auto it = get_service_callbacks_.find(service_name);
    if (it == get_service_callbacks_.end()) {
      LOG(WARNING) << "Received unknown service \"" << service_name << "\"";
      return 0;
    }

    it->second.Run(make_scoped_ptr(proxy.release()));
    return 0;
  }

 private:
  std::unique_ptr<BinderProxy> psyched_proxy_;
  std::unique_ptr<IPsyched> psyched_interface_;

  // Keyed by service name.
  std::map<std::string, GetServiceCallback> get_service_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

PsycheConnection::PsycheConnection() : impl_(new Impl) {}

PsycheConnection::~PsycheConnection() = default;

void PsycheConnection::SetProxyForTesting(
    std::unique_ptr<BinderProxy> psyched_proxy) {
  impl_->SetProxyForTesting(std::move(psyched_proxy));
}

bool PsycheConnection::Init() {
  return impl_->Init();
}

bool PsycheConnection::RegisterService(const std::string& service_name,
                                       BinderHost* service) {
  return impl_->RegisterService(service_name, service);
}

bool PsycheConnection::GetService(const std::string& service_name,
                                  const GetServiceCallback& callback) {
  return impl_->GetService(service_name, callback);
}

}  // namespace psyche
