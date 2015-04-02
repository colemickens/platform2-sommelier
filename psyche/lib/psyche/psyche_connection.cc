// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/lib/psyche/psyche_connection.h"

#include <map>

#include <base/logging.h>
#include <protobinder/binder_proxy.h>
#include <protobinder/ibinder.h>
#include <protobinder/iservice_manager.h>
#include <protobinder/protobinder.h>

#include "psyche/common/constants.h"
#include "psyche/common/util.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/proto_bindings/psyche.pb.rpc.h"

using protobinder::IBinder;
using protobinder::BinderHost;
using protobinder::BinderProxy;

namespace psyche {

class PsycheConnection::Impl : public IPsycheClientHostInterface {
 public:
  Impl() = default;
  ~Impl() override = default;

  bool Init() {
    psyched_binder_.reset(protobinder::GetServiceManager()->GetService(
        kPsychedServiceManagerName));
    if (!psyched_binder_) {
      LOG(ERROR) << "Failed to connect to psyched";
      return false;
    }
    psyched_interface_.reset(
        protobinder::BinderToInterface<IPsyched>(psyched_binder_.get()));
    return true;
  }

  bool RegisterService(const std::string& service_name, BinderHost* service) {
    DCHECK(psyched_interface_);

    RegisterServiceRequest request;
    request.set_name(service_name);
    util::CopyBinderToProto(*service, request.mutable_binder());

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

  void GetService(const std::string& service_name,
                  const GetServiceCallback& callback) {
    DCHECK(psyched_interface_);

    get_service_callbacks_[service_name] = callback;

    RequestServiceRequest request;
    request.set_name(service_name);
    util::CopyBinderToProto(*this, request.mutable_client_binder());

    RequestServiceResponse response;
    const int result = psyched_interface_->RequestService(&request, &response);
    if (result != SUCCESS) {
      LOG(ERROR) << "RequestService binder call failed with " << result;
      callback.Run(scoped_ptr<BinderProxy>());
      return;
    }
    if (!response.success()) {
      LOG(ERROR) << "psyched reported failure when requesting " << service_name;
      callback.Run(scoped_ptr<BinderProxy>());
      return;
    }
  }

  // IPsycheClientHostInterface:
  int ReceiveService(ReceiveServiceRequest* in,
                     ReceiveServiceResponse* out) override {
    const std::string service_name = in->name();
    std::unique_ptr<BinderProxy> proxy =
        util::ExtractBinderProxyFromProto(in->mutable_binder());

    auto it = get_service_callbacks_.find(service_name);
    if (it == get_service_callbacks_.end()) {
      LOG(WARNING) << "Received unknown service \"" << service_name << "\"";
      return 0;
    }

    it->second.Run(make_scoped_ptr(proxy.release()));
    return 0;
  }

 private:
  std::unique_ptr<IBinder> psyched_binder_;
  std::unique_ptr<IPsyched> psyched_interface_;

  // Keyed by service name.
  std::map<std::string, GetServiceCallback> get_service_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

PsycheConnection::PsycheConnection() = default;

PsycheConnection::~PsycheConnection() = default;

bool PsycheConnection::Init() {
  impl_.reset(new Impl);
  return impl_->Init();
}

bool PsycheConnection::RegisterService(const std::string& service_name,
                                       BinderHost* service) {
  return impl_->RegisterService(service_name, service);
}

void PsycheConnection::GetService(const std::string& service_name,
                                  const GetServiceCallback& callback) {
  impl_->GetService(service_name, callback);
}

}  // namespace psyche
