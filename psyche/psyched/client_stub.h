// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PSYCHE_PSYCHED_CLIENT_STUB_H_
#define PSYCHE_PSYCHED_CLIENT_STUB_H_

#include <map>
#include <memory>
#include <string>

#include "psyche/psyched/client.h"

namespace protobinder {
class BinderProxy;
}  // protobinder

namespace psyche {

// A stub implementation of ClientInterface used for testing.
class ClientStub : public ClientInterface {
 public:
  explicit ClientStub(std::unique_ptr<protobinder::BinderProxy> client_proxy);
  ~ClientStub() override;

  // Returns the number of times that a service request failure has been
  // reported for |service_name|.
  int GetServiceRequestFailures(const std::string& service_name) const;

  // ClientInterface:
  const ServiceSet& GetServices() const override;
  void ReportServiceRequestFailure(const std::string& service_name) override;
  void AddService(ServiceInterface* service) override;
  void RemoveService(ServiceInterface* service) override;

 private:
  std::unique_ptr<protobinder::BinderProxy> client_proxy_;
  ServiceSet services_;

  // Number of times that a service name has been passed to
  // ReportServiceRequestFailure().
  std::map<std::string, int> service_request_failures_;

  DISALLOW_COPY_AND_ASSIGN(ClientStub);
};

}  // namespace psyche

#endif  // PSYCHE_PSYCHED_CLIENT_STUB_H_
