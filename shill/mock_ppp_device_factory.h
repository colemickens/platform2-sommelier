// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PPP_DEVICE_FACTORY_H_
#define SHILL_MOCK_PPP_DEVICE_FACTORY_H_

#include <string>

#include <base/macros.h>
#include <base/no_destructor.h>
#include <gmock/gmock.h>

#include "shill/ppp_device_factory.h"

namespace shill {

class MockPPPDeviceFactory : public PPPDeviceFactory {
 public:
  ~MockPPPDeviceFactory() override;

  // This is a singleton. Use MockPPPDeviceFactory::GetInstance()->Foo().
  static MockPPPDeviceFactory* GetInstance();

  MOCK_METHOD6(CreatePPPDevice,
               PPPDevice* (ControlInterface* control,
                           EventDispatcher* dispatcher,
                           Metrics* metrics,
                           Manager* manager,
                           const std::string& link_name,
                           int interface_index));

 protected:
  MockPPPDeviceFactory();

 private:
  friend class base::NoDestructor<MockPPPDeviceFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockPPPDeviceFactory);
};

}  // namespace shill

#endif  // SHILL_MOCK_PPP_DEVICE_FACTORY_H_
