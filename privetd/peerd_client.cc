// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/peerd_client.h"

#include <map>

#include <base/strings/string_util.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"

namespace privetd {

PeerdClient::PeerdClient(const scoped_refptr<dbus::Bus>& bus,
                         const DeviceDelegate& device,
                         const CloudDelegate* cloud)
    : manager_proxy_{bus}, device_{device}, cloud_{cloud} {
}

PeerdClient::~PeerdClient() {
  Stop();
}

void PeerdClient::Start() {
  CHECK(service_token_.empty());
  chromeos::ErrorPtr error;

  const uint16_t port = device_.GetHttpEnpoint().first;
  std::map<std::string, chromeos::Any> mdns_options{
      {"port", chromeos::Any{port}},
  };

  DCHECK_NE(port, 0);
  DCHECK(!device_.GetName().empty());
  DCHECK(!device_.GetId().empty());
  DCHECK_EQ(device_.GetClass().size(), 2U);
  DCHECK_EQ(device_.GetModelId().size(), 3U);

  std::string services;
  if (!device_.GetServices().empty())
    services += "_";
  services += JoinString(device_.GetServices(), ",_");

  std::map<std::string, std::string> txt_record{
      {"txtver", "3"},
      {"ty", device_.GetName()},
      {"services", services},
      {"id", device_.GetId()},
      {"class", device_.GetClass()},
      {"modelId", device_.GetModelId()},
      {"flags", "DA"},  // TODO(vitalybuka): find owner for flags.
  };

  if (cloud_ && !cloud_->GetCloudId().empty())
    txt_record.emplace("gcd.id", cloud_->GetCloudId());

  if (!device_.GetDescription().empty())
    txt_record.emplace("description", device_.GetDescription());

  if (!manager_proxy_.ExposeService("privet", txt_record,
                                    {{"mdns", mdns_options}}, &service_token_,
                                    &error)) {
    LOG(ERROR) << "ExposeService failed:" << error->GetMessage();
  }
}

void PeerdClient::Stop() {
  if (service_token_.empty())
    return;
  chromeos::ErrorPtr error;

  if (!manager_proxy_.RemoveExposedService(service_token_, &error)) {
    LOG(ERROR) << "RemoveExposedService failed:" << error->GetMessage();
  }
  service_token_.clear();
}

}  // namespace privetd
