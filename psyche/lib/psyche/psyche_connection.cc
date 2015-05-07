// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/lib/psyche/psyche_connection.h"

#include <map>
#include <utility>

#include <base/logging.h>
#include <chromeos/make_unique_ptr.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/ibinder.h>
#include <protobinder/iservice_manager.h>
#include <protobinder/status.h>

#include "psyche/common/constants.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

using chromeos::make_unique_ptr;
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

  bool RegisterService(const std::string& service_name,
                       const BinderHost* service) {
    DCHECK(psyched_interface_);

    RegisterServiceRequest request;
    request.set_name(service_name);
    service->CopyToProtocolBuffer(request.mutable_binder());

    RegisterServiceResponse response;

    Status status = psyched_interface_->RegisterService(&request, &response);
    if (!status) {
      LOG(ERROR) << "RegisterService binder call failed with " << status;
      return false;
    }
    return true;
  }

  bool GetService(const std::string& service_name,
                  const GetServiceCallback& callback) {
    DCHECK(psyched_interface_);

    RequestServiceRequest request;
    request.set_name(service_name);
    CopyToProtocolBuffer(request.mutable_client_binder());
    Status status = psyched_interface_->RequestService(&request);
    if (!status) {
      LOG(ERROR) << "RequestService binder call failed with " << status;
      return false;
    }

    // Register the callback last so it doesn't stick around if the RPC failed.
    // This is safe to do since we're making a one-way call, i.e. psyched's
    // asynchronous ReceiveService call won't be handled until control returns
    // to the message loop.
    get_service_callbacks_.insert(std::make_pair(service_name, callback));
    return true;
  }

  // IPsycheClientHostInterface:
  Status ReceiveService(ReceiveServiceRequest* in) override {
    const std::string service_name = in->name();
    std::unique_ptr<BinderProxy> proxy;
    if (in->has_binder())
      proxy.reset(new BinderProxy(in->binder().proxy_handle()));

    auto range = get_service_callbacks_.equal_range(service_name);
    if (range.first == get_service_callbacks_.end()) {
      LOG(WARNING) << "Received unknown service \"" << service_name << "\"";
      return STATUS_OK();
    }

    for (auto it = range.first; it != range.second; ++it) {
      // Create a new BinderProxy for each callback based on the original
      // proxy's handle. The handle's references are incremented and decremented
      // in BinderProxy's c'tor and d'tor, so this is safe to do.
      it->second.Run(
          make_unique_ptr(proxy ? new BinderProxy(proxy->handle()) : nullptr));
    }
    return STATUS_OK();
  }

 private:
  std::unique_ptr<BinderProxy> psyched_proxy_;
  std::unique_ptr<IPsyched> psyched_interface_;

  // Keyed by service name.
  std::multimap<std::string, GetServiceCallback> get_service_callbacks_;

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
                                       const BinderHost* service) {
  return impl_->RegisterService(service_name, service);
}

bool PsycheConnection::GetService(const std::string& service_name,
                                  const GetServiceCallback& callback) {
  return impl_->GetService(service_name, callback);
}

}  // namespace psyche
