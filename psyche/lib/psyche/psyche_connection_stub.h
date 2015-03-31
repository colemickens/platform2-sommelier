// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_LIB_PSYCHE_PSYCHE_CONNECTION_STUB_H_
#define PSYCHE_LIB_PSYCHE_PSYCHE_CONNECTION_STUB_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <psyche/psyche_connection.h>
#include <psyche/psyche_export.h>

namespace protobinder {
class BinderHost;
class BinderProxy;
}

namespace psyche {

// Stub implementation of PsycheConnectionInterface used to test code that
// communicates with psyched.
class PSYCHE_EXPORT PsycheConnectionStub : public PsycheConnectionInterface {
 public:
  PsycheConnectionStub();
  ~PsycheConnectionStub() override;

  using ServiceHostMap = std::map<std::string, const protobinder::BinderHost*>;
  const ServiceHostMap& registered_services() const {
    return registered_services_;
  }

  void set_register_service_result(bool result) {
    register_service_result_ = result;
  }
  void set_get_service_result(bool result) { get_service_result_ = result; }

  // Posts tasks to run each callback that was previously registered for
  // |service_name| via GetService() with its own copy of |service_proxy| (i.e.
  // a new object with the same handle).
  void PostGetServiceTasks(const std::string& service_name,
                           const protobinder::BinderProxy* service_proxy);

  // PsycheConnectionInterface:
  bool RegisterService(
      const std::string& service_name,
      const protobinder::BinderHost* service) override WARN_UNUSED_RESULT;
  bool GetService(
      const std::string& service_name,
      const GetServiceCallback& callback) override WARN_UNUSED_RESULT;

 private:
  // Results synchronously returned by RegisterService() and GetService().
  bool register_service_result_;
  bool get_service_result_;

  // Services that have been registered via RegisterService(), keyed by service
  // name.
  ServiceHostMap registered_services_;

  // Keyed by service name.
  std::multimap<std::string, GetServiceCallback> get_service_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PsycheConnectionStub);
};

}  // namespace psyche

#endif  // PSYCHE_LIB_PSYCHE_PSYCHE_CONNECTION_STUB_H_
