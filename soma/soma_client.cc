// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/flag_helper.h>
#include <protobinder/binder_proxy.h>
#include <psyche/psyche_connection.h>
#include <psyche/psyche_daemon.h>

#include "soma/lib/soma/constants.h"
#include "soma/lib/soma/read_only_container_spec.h"
#include "soma/proto_bindings/soma.pb.h"
#include "soma/proto_bindings/soma.pb.rpc.h"
#include "soma/proto_bindings/soma_container_spec.pb.h"

namespace soma {

class ContainerSpecFetch : public psyche::PsycheDaemon {
 public:
  explicit ContainerSpecFetch(const std::string& name)
      : name_(name), weak_ptr_factory_(this) {}
  ~ContainerSpecFetch() override = default;

 private:
  void RequestService() {
    LOG(INFO) << "Requesting service " << kSomaServiceName;
    psyche_connection()->GetService(
        kSomaServiceName,
        base::Bind(&ContainerSpecFetch::ReceiveService,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void ReceiveService(scoped_ptr<BinderProxy> proxy) {
    LOG(INFO) << "Received service with handle " << proxy->handle();
    proxy_.reset(proxy.release());
    soma_.reset(protobinder::BinderToInterface<ISoma>(proxy_.get()));
    base::MessageLoopForIO::current()->PostTask(
        FROM_HERE, base::Bind(&ContainerSpecFetch::DoFetch,
                              weak_ptr_factory_.GetWeakPtr()));
  }

  void DoFetch() {
    DCHECK(soma_);
    GetContainerSpecRequest request;
    GetContainerSpecResponse response;
    request.set_service_name(name_);
    int binder_ret = soma_->GetContainerSpec(&request, &response);
    if (binder_ret != 0) {
      LOG(ERROR) << "Failed to get spec for '" << name_ << "'";
      Quit();
      return;
    }
    soma::ReadOnlyContainerSpec ro_spec(response.mutable_container_spec());
    LOG(INFO) << ro_spec.service_bundle_path().value();

    Quit();
  }

  // PsycheDaemon:
  int OnInit() override {
    int return_code = PsycheDaemon::OnInit();
    if (return_code != EX_OK)
      return return_code;

    base::MessageLoopForIO::current()->PostTask(
        FROM_HERE, base::Bind(&ContainerSpecFetch::RequestService,
                              weak_ptr_factory_.GetWeakPtr()));
    return EX_OK;
  }

  std::unique_ptr<BinderProxy> proxy_;
  std::unique_ptr<ISoma> soma_;

  std::string name_;

  // Keep this member last.
  base::WeakPtrFactory<ContainerSpecFetch> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContainerSpecFetch);
};

}  // namespace soma

int main(int argc, char* argv[]) {
  DEFINE_string(service_name, "",
                "Name of service for which to fetch a container spec.");
  chromeos::FlagHelper::Init(argc, argv, "Command-line client for somad.");
  return soma::ContainerSpecFetch(FLAGS_service_name).Run();
}
