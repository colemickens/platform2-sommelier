// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_PROCESS_FACTORY_H_
#define APMANAGER_MOCK_PROCESS_FACTORY_H_

#include <base/lazy_instance.h>
#include <gmock/gmock.h>

#include "apmanager/process_factory.h"

namespace apmanager {

class MockProcessFactory : public ProcessFactory {
 public:
  ~MockProcessFactory() override;

  // This is a singleton. Use MockDHCPServerFactory::GetInstance()->Foo().
  static MockProcessFactory* GetInstance();

  MOCK_METHOD0(CreateProcess, chromeos::Process*());

 protected:
  MockProcessFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockProcessFactory>;

  DISALLOW_COPY_AND_ASSIGN(MockProcessFactory);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_PROCESS_FACTORY_H_
