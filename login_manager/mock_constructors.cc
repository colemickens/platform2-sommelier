// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/message_loop_proxy.h>

#include "login_manager/mock_child_job.h"
#include "login_manager/mock_device_policy_service.h"
#include "login_manager/mock_file_checker.h"
#include "login_manager/mock_key_generator.h"
#include "login_manager/mock_mitigator.h"
#include "login_manager/mock_owner_key.h"
#include "login_manager/mock_policy_service.h"
#include "login_manager/mock_policy_store.h"
#include "login_manager/mock_system_utils.h"

// Per the gmock documentation, the vast majority of the time spent on
// compiling a mock class is in generating its constructor and
// destructor, as they perform non-trivial tasks.  To combat this, the
// docs recommend moving the definition of your mock class'
// constructor and destructor out of the class body and into a .cc
// file. This way, even if you #include your mock class in N files,
// the compiler only needs to generate its constructor and destructor
// once, resulting in a much faster compilation.
//
// To avoid having to add a bunch of boilerplate and a .cc file for every
// mock I define, I will simply collect the constructors all here.

namespace login_manager {
MockChildJob::MockChildJob() {
  EXPECT_CALL(*this, ShouldNeverKill())
      .WillRepeatedly(::testing::Return(false));
}
MockChildJob::~MockChildJob() {}

MockDevicePolicyService::MockDevicePolicyService()
  : DevicePolicyService(NULL, NULL, NULL, NULL, NULL, NULL) {}
MockDevicePolicyService::~MockDevicePolicyService() {}

MockFileChecker::MockFileChecker(std::string filename)
  : FileChecker(filename) {}
MockFileChecker::~MockFileChecker() {}

MockKeyGenerator::MockKeyGenerator() {}
MockKeyGenerator::~MockKeyGenerator() {}

MockMitigator::MockMitigator() {}
MockMitigator::~MockMitigator() {}

MockOwnerKey::MockOwnerKey() : OwnerKey(FilePath("")) {}
MockOwnerKey::~MockOwnerKey() {}

MockPolicyService::MockPolicyService()
  : PolicyService(NULL, NULL, NULL, NULL) {}
MockPolicyService::~MockPolicyService() {}

MockPolicyServiceCompletion::MockPolicyServiceCompletion() {}
MockPolicyServiceCompletion::~MockPolicyServiceCompletion() {}

MockPolicyServiceDelegate::MockPolicyServiceDelegate() {}
MockPolicyServiceDelegate::~MockPolicyServiceDelegate() {}

MockPolicyStore::MockPolicyStore() : PolicyStore(FilePath("")) {}
MockPolicyStore::~MockPolicyStore() {}

MockSystemUtils::MockSystemUtils() {}
MockSystemUtils::~MockSystemUtils() {}

}  // namespace login_manager
