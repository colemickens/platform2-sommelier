// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_POLICY_SERVICE_H_
#define LOGIN_MANAGER_MOCK_POLICY_SERVICE_H_

#include "login_manager/policy_service.h"

#include <stdint.h>

#include <string>
#include <vector>

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>

namespace login_manager {
MATCHER_P(PolicyErrorEq, error_code, "") {
  return (arg.code() == error_code);
}

class MockPolicyService : public PolicyService {
 public:
  MockPolicyService();
  virtual ~MockPolicyService();
  MOCK_METHOD5(Store, bool(const uint8_t*, uint32_t, const Completion&, int,
                           SignatureCheck));
  MOCK_METHOD1(Retrieve, bool(std::vector<uint8_t>*));

  static Completion CreateDoNothing() {
    return base::Bind(&MockPolicyService::DoNothingWithError);
  }

  static Completion CreateExpectSuccessCallback() {
    return base::Bind(&ExpectingErrorHandler::HandleError,
                      base::Owned(new ExpectingErrorHandler(true)));
  }

  static Completion CreateExpectFailureCallback() {
    return base::Bind(&ExpectingErrorHandler::HandleError,
                      base::Owned(new ExpectingErrorHandler(false)));
  }

 private:
  // One downside of reporting results via a callback is that there's no longer
  // a handy object to mock out and set expectations on. You can provide a
  // callback in tests that checks a value passed in, but if that callback is
  // never run, the check will never happen! By having an actual object with
  // a mocked out method which can be owned by the callback, we can set an
  // expectation that remains unmet if the callback is not run before
  // destruction. This class supports very basic success-or-failure expectations
  // and the helper methods above vend appropriate callbacks.
  class ExpectingErrorHandler {
   public:
    explicit ExpectingErrorHandler(bool expect_match) {
      using testing::Not;
      if (expect_match)
        EXPECT_CALL(*this, HandleError(PolicyErrorEq(dbus_error::kNone)));
      else
        EXPECT_CALL(*this, HandleError(Not(PolicyErrorEq(dbus_error::kNone))));
    }
    virtual ~ExpectingErrorHandler() {}

    MOCK_METHOD1(HandleError, void(const Error&));
   private:
    DISALLOW_COPY_AND_ASSIGN(ExpectingErrorHandler);
  };

  static void DoNothingWithError(const Error&) {}
};

class MockPolicyServiceDelegate : public PolicyService::Delegate {
 public:
  MockPolicyServiceDelegate();
  virtual ~MockPolicyServiceDelegate();
  MOCK_METHOD1(OnPolicyPersisted, void(bool));
  MOCK_METHOD1(OnKeyPersisted, void(bool));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_POLICY_SERVICE_H_
