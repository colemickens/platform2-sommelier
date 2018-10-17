// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connectivity_trial.h"

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <brillo/http/mock_connection.h>
#include <brillo/http/mock_transport.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_http_request.h"
#include "shill/net/mock_time.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;
using std::unique_ptr;
using std::vector;
using testing::_;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {
const char kBadURL[] = "badurl";
const char kInterfaceName[] = "int0";
const char kHttpUrl[] = "http://www.chromium.org";
const char kHttpsUrl[] = "https://www.google.com";
const char kDNSServer0[] = "8.8.8.8";
const char kDNSServer1[] = "8.8.4.4";
const char* const kDNSServers[] = {kDNSServer0, kDNSServer1};
}  // namespace

MATCHER_P(IsResult, result, "") {
  return (result.phase == arg.phase &&
          result.status == arg.status);
}

class ConnectivityTrialTest : public Test {
 public:
  ConnectivityTrialTest()
      : device_info_(
            new NiceMock<MockDeviceInfo>(&control_, nullptr, nullptr, nullptr)),
        connection_(new StrictMock<MockConnection>(device_info_.get())),
        transport_(std::make_shared<brillo::http::MockTransport>()),
        brillo_connection_(
            std::make_shared<brillo::http::MockConnection>(transport_)),
        connectivity_trial_(
            new ConnectivityTrial(connection_.get(),
                                  &dispatcher_,
                                  kTrialTimeout,
                                  callback_target_.result_callback())),
        interface_name_(kInterfaceName),
        dns_servers_(kDNSServers, kDNSServers + 2),
        http_request_(nullptr) {
    current_time_.tv_sec = current_time_.tv_usec = 0;
  }

  void SetUp() override {
    EXPECT_CALL(*connection_, IsIPv6()).WillRepeatedly(Return(false));
    EXPECT_CALL(*connection_, interface_name())
        .WillRepeatedly(ReturnRef(interface_name_));
    EXPECT_CALL(time_, GetTimeMonotonic(_))
        .WillRepeatedly(Invoke(this, &ConnectivityTrialTest::GetTimeMonotonic));
    EXPECT_CALL(*connection_, dns_servers())
        .WillRepeatedly(ReturnRef(dns_servers_));
    EXPECT_FALSE(connectivity_trial_->http_request_.get());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(&http_request_);
    if (connectivity_trial_->http_request_.get()) {
      EXPECT_CALL(*http_request(), Stop());

      // Delete the ConnectivityTrial while expectations still exist.
      connectivity_trial_.reset();
    }
    testing::Mock::VerifyAndClearExpectations(brillo_connection_.get());
    brillo_connection_.reset();
    testing::Mock::VerifyAndClearExpectations(transport_.get());
    transport_.reset();
  }

 protected:
  static const int kNumAttempts;
  static const int kTrialTimeout;

  class CallbackTarget {
   public:
    CallbackTarget()
        : result_callback_(Bind(&CallbackTarget::ResultCallback,
                                Unretained(this))) {
    }

    MOCK_METHOD1(ResultCallback, void(ConnectivityTrial::Result result));
    Callback<void(ConnectivityTrial::Result)>& result_callback() {
      return result_callback_;
    }

   private:
    Callback<void(ConnectivityTrial::Result)> result_callback_;
  };

  void AssignHttpRequest() {
    http_request_ = new StrictMock<MockHttpRequest>(connection_);
    connectivity_trial_->http_request_.reset(
        http_request_);  // Passes ownership.
  }

  bool StartTrialWithDelay(
      const ConnectivityTrial::PortalDetectionProperties& props, int delay) {
    bool ret = connectivity_trial_->Start(props, delay);
    if (ret) {
      AssignHttpRequest();
    }
    return ret;
  }

  bool StartTrial(const ConnectivityTrial::PortalDetectionProperties& props) {
    return StartTrialWithDelay(props, 0);
  }

  void StartTrialTask() {
    AssignHttpRequest();
    EXPECT_CALL(*http_request(), Start(_, _, _))
        .WillOnce(Return(HttpRequest::kResultInProgress));
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, kTrialTimeout * 1000));
    connectivity_trial()->StartTrialTask();
  }

  void ExpectTrialReturn(const ConnectivityTrial::Result& result) {
    EXPECT_CALL(callback_target(), ResultCallback(IsResult(result)));

    // Expect the PortalDetector to stop the current request.
    EXPECT_CALL(*http_request(), Stop());
  }

  void TimeoutTrial() {
    connectivity_trial_->TimeoutTrialTask();
  }

  MockHttpRequest* http_request() { return http_request_; }
  ConnectivityTrial* connectivity_trial() { return connectivity_trial_.get(); }
  MockEventDispatcher& dispatcher() { return dispatcher_; }
  CallbackTarget& callback_target() { return callback_target_; }

  void ExpectReset() {
    EXPECT_TRUE(callback_target_.result_callback().
                Equals(connectivity_trial_->trial_callback_));
    EXPECT_FALSE(connectivity_trial_->http_request_.get());
  }

  void ExpectTrialRetry(const ConnectivityTrial::Result& result, int delay) {
    EXPECT_CALL(callback_target(), ResultCallback(IsResult(result)));

    // Expect the ConnectivityTrial to stop the current request.
    EXPECT_CALL(*http_request(), Stop());

    // Expect the ConnectivityTrial to schedule the next attempt.
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, delay));
  }

  void AdvanceTime(int milliseconds) {
    struct timeval tv = { milliseconds / 1000, (milliseconds % 1000) * 1000 };
    timeradd(&current_time_, &tv, &current_time_);
  }

  void StartTrial() {
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
    ConnectivityTrial::PortalDetectionProperties props =
        ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
    EXPECT_TRUE(StartTrial(props));

    // Expect that the request will be started -- return failure.
    EXPECT_CALL(*http_request(), Start(_, _, _))
        .WillOnce(Return(HttpRequest::kResultInProgress));
    EXPECT_CALL(dispatcher(), PostDelayedTask(
        _, _, kTrialTimeout * 1000));

    connectivity_trial()->StartTrialTask();
  }

  void ExpectRequestSuccessWithStatus(int status_code) {
    EXPECT_CALL(*brillo_connection_, GetResponseStatusCode())
        .WillOnce(Return(status_code));

    auto response =
        std::make_shared<brillo::http::Response>(brillo_connection_);
    connectivity_trial_->RequestSuccessCallback(response);
  }

 private:
  int GetTimeMonotonic(struct timeval* tv) {
    *tv = current_time_;
    return 0;
  }

  StrictMock<MockEventDispatcher> dispatcher_;
  MockControl control_;
  unique_ptr<MockDeviceInfo> device_info_;
  scoped_refptr<MockConnection> connection_;
  std::shared_ptr<brillo::http::MockTransport> transport_;
  std::shared_ptr<brillo::http::MockConnection> brillo_connection_;
  CallbackTarget callback_target_;
  unique_ptr<ConnectivityTrial> connectivity_trial_;
  StrictMock<MockTime> time_;
  struct timeval current_time_;
  const string interface_name_;
  vector<string> dns_servers_;
  MockHttpRequest* http_request_;
};

// static
const int ConnectivityTrialTest::kNumAttempts = 0;
const int ConnectivityTrialTest::kTrialTimeout = 4;

TEST_F(ConnectivityTrialTest, Constructor) {
  ExpectReset();
}

TEST_F(ConnectivityTrialTest, InvalidURL) {
  EXPECT_FALSE(connectivity_trial()->IsActive());
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(0);
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kBadURL, kHttpsUrl);
  EXPECT_FALSE(StartTrial(props));
  ExpectReset();

  EXPECT_FALSE(connectivity_trial()->Retry(0));
  EXPECT_FALSE(connectivity_trial()->IsActive());
}

TEST_F(ConnectivityTrialTest, IsActive) {
  // Before the trial is started, should not be active.
  EXPECT_FALSE(connectivity_trial()->IsActive());

  // Once the trial is started, IsActive should return true.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));
  StartTrialTask();
  EXPECT_TRUE(connectivity_trial()->IsActive());

  // Finish the trial, IsActive should return false.
  EXPECT_CALL(*http_request(), Stop());
  connectivity_trial()->CompleteTrial(
      ConnectivityTrial::Result(ConnectivityTrial::kPhaseContent,
                                ConnectivityTrial::kStatusFailure));
  EXPECT_FALSE(connectivity_trial()->IsActive());
}

TEST_F(ConnectivityTrialTest, StartAttemptFailed) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  // Expect that the request will be started -- return failure.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HttpRequest::kResultDNSFailure));
  // Expect a failure to be relayed to the caller.
  EXPECT_CALL(
      callback_target(),
      ResultCallback(IsResult(ConnectivityTrial::Result(
          ConnectivityTrial::kPhaseDNS, ConnectivityTrial::kStatusFailure))))
      .Times(1);

  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(0);
  EXPECT_CALL(*http_request(), Stop());

  connectivity_trial()->StartTrialTask();
}

TEST_F(ConnectivityTrialTest, StartRepeated) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(1);
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  // A second call should cancel the existing trial and set up the new one.
  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 10)).Times(1);
  EXPECT_TRUE(StartTrialWithDelay(props, 10));
}

TEST_F(ConnectivityTrialTest, StartTrialAfterDelay) {
  const int kDelaySeconds = 123;
  // The trial should be delayed by kDelaySeconds.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, kDelaySeconds));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrialWithDelay(props, kDelaySeconds));
}

TEST_F(ConnectivityTrialTest, TrialRetry) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  // Expect that the request will be started -- return failure.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HttpRequest::kResultConnectionFailure));
  EXPECT_CALL(*http_request(), Stop());
  connectivity_trial()->StartTrialTask();

  const int kRetryDelay = 7;
  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, kRetryDelay)).Times(1);
  EXPECT_TRUE(connectivity_trial()->Retry(kRetryDelay));
}

TEST_F(ConnectivityTrialTest, TrialRetryFail) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  EXPECT_CALL(*http_request(), Stop());
  connectivity_trial()->Stop();

  EXPECT_FALSE(connectivity_trial()->Retry(0));
}

// Exactly like AttemptCount, except that the termination conditions are
// different because we're triggering a different sort of error.
TEST_F(ConnectivityTrialTest, ReadBadHeadersRetry) {
  int num_failures = 3;
  int sec_between_attempts = 3;

  // Expect ConnectivityTrial to immediately post a task for the each attempt.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  // Expect that the request will be started and return the in progress status.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .Times(num_failures).WillRepeatedly(
          Return(HttpRequest::kResultInProgress));

  // Each HTTP request that gets started will have a request timeout.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, kTrialTimeout * 1000))
      .Times(num_failures);

  // Expect failures for all attempts but the last.
  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(
                  ConnectivityTrial::Result(
                      ConnectivityTrial::kPhaseContent,
                      ConnectivityTrial::kStatusFailure))))
      .Times(num_failures);

  // Expect the ConnectivityTrial to stop the current request each time, plus
  // an extra time in ConnectivityTrial::Stop().

  for (int i = 0; i < num_failures; ++i) {
    connectivity_trial()->StartTrialTask();
    AdvanceTime(sec_between_attempts * 1000);
    EXPECT_CALL(*http_request(), Stop()).Times(2);
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(1);
    ExpectRequestSuccessWithStatus(123);
    EXPECT_TRUE(connectivity_trial()->Retry(0));
  }
}

TEST_F(ConnectivityTrialTest, RequestTimeout) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  StartTrialTask();

  ExpectTrialReturn(ConnectivityTrial::Result(
      ConnectivityTrial::kPhaseUnknown,
      ConnectivityTrial::kStatusTimeout));

  TimeoutTrial();
}

TEST_F(ConnectivityTrialTest, RequestSuccess) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  StartTrialTask();

  ExpectTrialReturn(ConnectivityTrial::Result(
      ConnectivityTrial::kPhaseContent,
      ConnectivityTrial::kStatusSuccess));

  ExpectRequestSuccessWithStatus(204);
}

TEST_F(ConnectivityTrialTest, RequestFail) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  ConnectivityTrial::PortalDetectionProperties props =
      ConnectivityTrial::PortalDetectionProperties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartTrial(props));

  StartTrialTask();

  ExpectTrialReturn(ConnectivityTrial::Result(
      ConnectivityTrial::kPhaseContent, ConnectivityTrial::kStatusFailure));

  ExpectRequestSuccessWithStatus(123);
}

struct ResultMapping {
  ResultMapping() : http_result(HttpRequest::kResultUnknown), trial_result() {}
  ResultMapping(HttpRequest::Result in_http_result,
                const ConnectivityTrial::Result& in_trial_result)
      : http_result(in_http_result),
        trial_result(in_trial_result) {}
  HttpRequest::Result http_result;
  ConnectivityTrial::Result trial_result;
};

class ConnectivityTrialResultMappingTest
    : public testing::TestWithParam<ResultMapping> {};

TEST_P(ConnectivityTrialResultMappingTest, MapResult) {
  ConnectivityTrial::Result trial_result =
      ConnectivityTrial::GetPortalResultForRequestResult(
          GetParam().http_result);
  EXPECT_EQ(trial_result.phase, GetParam().trial_result.phase);
  EXPECT_EQ(trial_result.status, GetParam().trial_result.status);
}

INSTANTIATE_TEST_CASE_P(
    TrialResultMappingTest,
    ConnectivityTrialResultMappingTest,
    ::testing::Values(ResultMapping(HttpRequest::kResultUnknown,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseUnknown,
                                        ConnectivityTrial::kStatusFailure)),
                      ResultMapping(HttpRequest::kResultInProgress,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseUnknown,
                                        ConnectivityTrial::kStatusFailure)),
                      ResultMapping(HttpRequest::kResultDNSFailure,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseDNS,
                                        ConnectivityTrial::kStatusFailure)),
                      ResultMapping(HttpRequest::kResultDNSTimeout,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseDNS,
                                        ConnectivityTrial::kStatusTimeout)),
                      ResultMapping(HttpRequest::kResultConnectionFailure,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseConnection,
                                        ConnectivityTrial::kStatusFailure)),
                      ResultMapping(HttpRequest::kResultHTTPFailure,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseHTTP,
                                        ConnectivityTrial::kStatusFailure)),
                      ResultMapping(HttpRequest::kResultHTTPTimeout,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseHTTP,
                                        ConnectivityTrial::kStatusTimeout)),
                      ResultMapping(HttpRequest::kResultSuccess,
                                    ConnectivityTrial::Result(
                                        ConnectivityTrial::kPhaseContent,
                                        ConnectivityTrial::kStatusFailure))));

}  // namespace shill
