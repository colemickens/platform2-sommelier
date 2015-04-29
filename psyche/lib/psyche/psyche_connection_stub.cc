// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "psyche/lib/psyche/psyche_connection_stub.h"

#include <utility>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/make_unique_ptr.h>
#include <protobinder/binder_proxy.h>

using chromeos::make_unique_ptr;
using protobinder::BinderHost;
using protobinder::BinderProxy;

namespace psyche {

PsycheConnectionStub::PsycheConnectionStub()
    : register_service_result_(true), get_service_result_(true) {}

PsycheConnectionStub::~PsycheConnectionStub() = default;

void PsycheConnectionStub::PostGetServiceTasks(const std::string& service_name,
                                               const BinderProxy* proxy) {
  auto range = get_service_callbacks_.equal_range(service_name);
  if (range.first == get_service_callbacks_.end())
    return;

  for (auto it = range.first; it != range.second; ++it) {
    base::MessageLoopForIO::current()->PostTask(
        FROM_HERE,
        base::Bind(it->second,
                   base::Passed(make_unique_ptr(
                       proxy ? new BinderProxy(proxy->handle()) : nullptr))));
  }
}

bool PsycheConnectionStub::RegisterService(const std::string& service_name,
                                           const BinderHost* service) {
  if (registered_services_.count(service_name))
    LOG(FATAL) << "Service \"" << service_name << "\" was registered twice";
  registered_services_[service_name] = service;
  return register_service_result_;
}

bool PsycheConnectionStub::GetService(const std::string& service_name,
                                      const GetServiceCallback& callback) {
  if (!get_service_result_)
    return false;
  get_service_callbacks_.insert(std::make_pair(service_name, callback));
  return true;
}

}  // namespace psyche
