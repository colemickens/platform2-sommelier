// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/portal_detector.h"

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <brillo/http/http_request.h>
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
using std::vector;
using testing::_;
using testing::InSequence;
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
  return (result.phase == arg.phase && result.status == arg.status &&
          result.final == arg.final);
}


class PortalDetectorTest : public Test {
 public:
  PortalDetectorTest()
      : device_info_(
            new NiceMock<MockDeviceInfo>(&control_, nullptr, nullptr, nullptr)),
        connection_(new StrictMock<MockConnection>(device_info_.get())),
        transport_(std::make_shared<brillo::http::MockTransport>()),
        brillo_connection_(
            std::make_shared<brillo::http::MockConnection>(transport_)),
        portal_detector_(
            new PortalDetector(connection_.get(),
                               &dispatcher_,
                               callback_target_.result_callback())),
        interface_name_(kInterfaceName),
        dns_servers_(kDNSServers, kDNSServers + 2),
        http_request_(nullptr),
        https_request_(nullptr) {
    current_time_.tv_sec = current_time_.tv_usec = 0;
  }

  void SetUp() override {
    EXPECT_CALL(*connection_, IsIPv6()).WillRepeatedly(Return(false));
    EXPECT_CALL(*connection_, interface_name())
        .WillRepeatedly(ReturnRef(interface_name_));
    portal_detector_->time_ = &time_;
    EXPECT_CALL(time_, GetTimeMonotonic(_))
        .WillRepeatedly(Invoke(this, &PortalDetectorTest::GetTimeMonotonic));
    EXPECT_CALL(*connection_, dns_servers())
        .WillRepeatedly(ReturnRef(dns_servers_));
    EXPECT_FALSE(portal_detector_->http_request_.get());
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(&http_request_);
    if (portal_detector()->http_request_) {
      EXPECT_CALL(*http_request(), Stop());
      EXPECT_CALL(*https_request(), Stop());

      // Delete the portal detector while expectations still exist.
      portal_detector_.reset();
    }
    testing::Mock::VerifyAndClearExpectations(brillo_connection_.get());
    brillo_connection_.reset();
    testing::Mock::VerifyAndClearExpectations(transport_.get());
    transport_.reset();
  }

 protected:
  static const int kNumAttempts;

  class CallbackTarget {
   public:
    CallbackTarget()
        : result_callback_(Bind(&CallbackTarget::ResultCallback,
                                Unretained(this))) {
    }

    MOCK_METHOD1(ResultCallback, void(const PortalDetector::Result& result));
    Callback<void(const PortalDetector::Result&)>& result_callback() {
      return result_callback_;
    }

   private:
    Callback<void(const PortalDetector::Result&)> result_callback_;
  };

  void AssignHttpRequest() {
    http_request_ = new StrictMock<MockHttpRequest>(connection_);
    https_request_ = new StrictMock<MockHttpRequest>(connection_);
    portal_detector_->http_request_.reset(http_request_);
    portal_detector_->https_request_.reset(
        https_request_);  // Passes ownership.
  }

  bool StartPortalRequest(const PortalDetector::Properties& props, int delay) {
    bool ret = portal_detector_->StartAfterDelay(props, delay);
    if (ret) {
      AssignHttpRequest();
    }
    return ret;
  }

  void StartTrialTask() {
    EXPECT_CALL(*http_request(), Start(_, _, _))
        .WillOnce(Return(HttpRequest::kResultInProgress));
    EXPECT_CALL(*https_request(), Start(_, _, _))
        .WillOnce(Return(HttpRequest::kResultInProgress));
    EXPECT_CALL(
        dispatcher(),
        PostDelayedTask(_, _, PortalDetector::kRequestTimeoutSeconds * 1000));
    portal_detector()->StartTrialTask();
  }

  MockHttpRequest* http_request() { return http_request_; }
  MockHttpRequest* https_request() { return https_request_; }
  PortalDetector* portal_detector() { return portal_detector_.get(); }
  MockEventDispatcher& dispatcher() { return dispatcher_; }
  CallbackTarget& callback_target() { return callback_target_; }

  void ExpectReset() {
    EXPECT_FALSE(portal_detector_->attempt_count_);
    EXPECT_FALSE(portal_detector_->failures_in_content_phase_);
    EXPECT_TRUE(callback_target_.result_callback().
                Equals(portal_detector_->portal_result_callback_));
    EXPECT_FALSE(portal_detector_->http_request_.get());
    EXPECT_FALSE(portal_detector_->https_request_.get());
  }

  void ExpectAttemptRetry(const PortalDetector::Result& result) {
    EXPECT_CALL(callback_target(), ResultCallback(IsResult(result)));

    // Expect the trial to schedule the next attempt.
    EXPECT_CALL(
        dispatcher(),
        PostDelayedTask(_, _,
                        PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000));
  }

  void AdvanceTime(int milliseconds) {
    struct timeval tv = { milliseconds / 1000, (milliseconds % 1000) * 1000 };
    timeradd(&current_time_, &tv, &current_time_);
  }

  void StartAttempt() {
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
    PortalDetector::Properties props =
        PortalDetector::Properties(kHttpUrl, kHttpsUrl);
    EXPECT_TRUE(StartPortalRequest(props, 0));
    StartTrialTask();
  }

  void StartSingleTrial() {
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
    PortalDetector::Properties props =
        PortalDetector::Properties(kHttpUrl, kHttpsUrl);
    EXPECT_TRUE(portal_detector()->StartSingleTrial(props));
    EXPECT_TRUE(portal_detector()->single_trial_);
    AssignHttpRequest();
  }

  void ExpectRequestSuccessWithStatus(int status_code, bool is_http) {
    EXPECT_CALL(*brillo_connection_, GetResponseStatusCode())
        .WillOnce(Return(status_code));

    auto response =
        std::make_shared<brillo::http::Response>(brillo_connection_);
    if (is_http)
      portal_detector_->HttpRequestSuccessCallback(response);
    else
      portal_detector_->HttpsRequestSuccessCallback(response);
  }

 private:
  int GetTimeMonotonic(struct timeval* tv) {
    *tv = current_time_;
    return 0;
  }

  StrictMock<MockEventDispatcher> dispatcher_;
  MockControl control_;
  std::unique_ptr<MockDeviceInfo> device_info_;
  scoped_refptr<MockConnection> connection_;
  std::shared_ptr<brillo::http::MockTransport> transport_;
  std::shared_ptr<brillo::http::MockConnection> brillo_connection_;
  CallbackTarget callback_target_;
  std::unique_ptr<PortalDetector> portal_detector_;
  StrictMock<MockTime> time_;
  struct timeval current_time_;
  const string interface_name_;
  vector<string> dns_servers_;
  MockHttpRequest* http_request_;
  MockHttpRequest* https_request_;
};

// static
const int PortalDetectorTest::kNumAttempts = 0;

TEST_F(PortalDetectorTest, Constructor) {
  ExpectReset();
}

TEST_F(PortalDetectorTest, InvalidURL) {
  EXPECT_FALSE(portal_detector()->IsActive());
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(0);
  PortalDetector::Properties props =
      PortalDetector::Properties(kBadURL, kHttpsUrl);
  EXPECT_FALSE(StartPortalRequest(props, 0));
  ExpectReset();

  EXPECT_FALSE(portal_detector()->Retry(0));
  EXPECT_FALSE(portal_detector()->IsActive());
}

TEST_F(PortalDetectorTest, IsActive) {
  // Before the trial is started, should not be active.
  EXPECT_FALSE(portal_detector()->IsActive());

  // Once the trial is started, IsActive should return true.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 0));

  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 3000));
  StartTrialTask();
  EXPECT_TRUE(portal_detector()->IsActive());

  // Finish the trial, IsActive should return false.
  EXPECT_CALL(*http_request(), Stop()).Times(2);
  EXPECT_CALL(*https_request(), Stop()).Times(2);
  portal_detector()->CompleteTrial(PortalDetector::Result(
      PortalDetector::Phase::kContent, PortalDetector::Status::kFailure));
  EXPECT_FALSE(portal_detector()->IsActive());
}

TEST_F(PortalDetectorTest, StartAttemptFailed) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 0));

  // Expect that the request will be started -- return failure.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HttpRequest::kResultDNSFailure));

  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(0);
  EXPECT_CALL(*http_request(), Stop()).Times(2);
  EXPECT_CALL(*https_request(), Stop()).Times(2);

  // Expect a non-final failure to be relayed to the caller.
  ExpectAttemptRetry(PortalDetector::Result(PortalDetector::Phase::kDNS,
                                            PortalDetector::Status::kFailure,
                                            kNumAttempts, false));

  portal_detector()->StartTrialTask();
}

TEST_F(PortalDetectorTest, AdjustStartDelayImmediate) {
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  EXPECT_TRUE(StartPortalRequest(props, 0));

  // A second attempt should be delayed by kMinTimeBetweenAttemptsSeconds.
  EXPECT_TRUE(portal_detector()->AdjustStartDelay(0)
              == PortalDetector::kMinTimeBetweenAttemptsSeconds);
}

TEST_F(PortalDetectorTest, AdjustStartDelayAfterDelay) {
  const int kDelaySeconds = 123;
  // The first attempt should be delayed by kDelaySeconds.
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, kDelaySeconds * 1000));

  EXPECT_TRUE(StartPortalRequest(props, kDelaySeconds));

  AdvanceTime(kDelaySeconds * 1000);

  // A second attempt should be delayed by kMinTimeBetweenAttemptsSeconds.
  EXPECT_TRUE(portal_detector()->AdjustStartDelay(0)
              == PortalDetector::kMinTimeBetweenAttemptsSeconds);
}

TEST_F(PortalDetectorTest, StartRepeated) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0)).Times(1);
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 0));

  // A second call should cancel the existing trial and set up the new one.
  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 10 * 1000)).Times(1);
  EXPECT_TRUE(portal_detector()->StartAfterDelay(props, 10));
}

TEST_F(PortalDetectorTest, TrialRetry) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 0));

  // Expect that the request will be started -- return failure.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HttpRequest::kResultConnectionFailure));
  EXPECT_CALL(*http_request(), Stop()).Times(2);
  EXPECT_CALL(*https_request(), Stop()).Times(2);
  EXPECT_CALL(dispatcher(),
              PostDelayedTask(
                  _, _, PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000));
  portal_detector()->StartTrialTask();

  const int kRetryDelay = 7;
  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, kRetryDelay)).Times(1);
  EXPECT_TRUE(portal_detector()->Retry(kRetryDelay));
}

TEST_F(PortalDetectorTest, TrialRetryFail) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, 0));
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 0));

  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  portal_detector()->Stop();

  EXPECT_FALSE(portal_detector()->Retry(0));
}

TEST_F(PortalDetectorTest, AttemptCount) {
  EXPECT_FALSE(portal_detector()->IsInProgress());
  // Expect the PortalDetector to immediately post a task for the each attempt.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, _)).Times(4);
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 2));

  {
    InSequence s;

    // Expect non-final failures for all attempts but the last.
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(PortalDetector::Result(
                    PortalDetector::Phase::kDNS,
                    PortalDetector::Status::kFailure, kNumAttempts, false))))
        .Times(PortalDetector::kMaxRequestAttempts - 1);

    // Expect a single final failure.
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(PortalDetector::Result(
                    PortalDetector::Phase::kDNS,
                    PortalDetector::Status::kFailure, kNumAttempts, true))))
        .Times(1);
  }

  // Expect the PortalDetector to stop the trial after
  // the final attempt.
  EXPECT_CALL(*http_request(), Stop()).Times(4);
  EXPECT_CALL(*https_request(), Stop()).Times(4);

  portal_detector()->StartAfterDelay(props, 0);
  for (int i = 0; i < PortalDetector::kMaxRequestAttempts; i++) {
    AdvanceTime(PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000);
    PortalDetector::Result r = PortalDetector::GetPortalResultForRequestResult(
        HttpRequest::kResultDNSFailure);
    portal_detector()->CompleteAttempt(r);
  }

  ExpectReset();
}

// // Exactly like AttemptCount, except that the termination conditions are
// // different because we're triggering a different sort of error.
TEST_F(PortalDetectorTest, ReadBadHeadersRetry) {
  EXPECT_FALSE(portal_detector()->IsInProgress());
  // Expect the PortalDetector to immediately post a task for the each attempt.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, _)).Times(3);
  PortalDetector::Properties props =
      PortalDetector::Properties(kHttpUrl, kHttpsUrl);
  EXPECT_TRUE(StartPortalRequest(props, 0));

  {
    InSequence s;

    // Expect non-final failures for all attempts but the last.
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(PortalDetector::Result(
                    PortalDetector::Phase::kContent,
                    PortalDetector::Status::kFailure, kNumAttempts, false))))
        .Times(PortalDetector::kMaxFailuresInContentPhase - 1);

    // Expect a single final failure.
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(PortalDetector::Result(
                    PortalDetector::Phase::kContent,
                    PortalDetector::Status::kFailure, kNumAttempts, true))))
        .Times(1);
  }

  // Expect the PortalDetector to stop the current request each time, plus
  // an extra time in PortalDetector::Stop().
  EXPECT_CALL(*http_request(), Stop()).Times(3);
  EXPECT_CALL(*https_request(), Stop()).Times(3);

  portal_detector()->StartAfterDelay(props, 0);
  for (int i = 0; i < PortalDetector::kMaxFailuresInContentPhase; i++) {
    AdvanceTime(PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000);
    PortalDetector::Result r = PortalDetector::Result(
        PortalDetector::Phase::kContent, PortalDetector::Status::kFailure);
    portal_detector()->CompleteAttempt(r);
  }

  EXPECT_FALSE(portal_detector()->IsInProgress());
}

TEST_F(PortalDetectorTest, ReadBadHeader) {
  StartAttempt();

  ExpectAttemptRetry(PortalDetector::Result(PortalDetector::Phase::kContent,
                                            PortalDetector::Status::kFailure,
                                            kNumAttempts, false));

  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  PortalDetector::Result r = PortalDetector::Result(
      PortalDetector::Phase::kContent, PortalDetector::Status::kFailure);
  portal_detector()->CompleteAttempt(r);
}

TEST_F(PortalDetectorTest, RequestTimeout) {
  StartAttempt();
  ExpectAttemptRetry(PortalDetector::Result(PortalDetector::Phase::kUnknown,
                                            PortalDetector::Status::kTimeout,
                                            kNumAttempts, false));

  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  PortalDetector::Result r = PortalDetector::Result(
      PortalDetector::Phase::kUnknown, PortalDetector::Status::kTimeout);
  portal_detector()->CompleteAttempt(r);
}

TEST_F(PortalDetectorTest, ReadPartialHeaderTimeout) {
  StartAttempt();

  ExpectAttemptRetry(PortalDetector::Result(PortalDetector::Phase::kContent,
                                            PortalDetector::Status::kTimeout,
                                            kNumAttempts, false));

  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  PortalDetector::Result r = PortalDetector::Result(
      PortalDetector::Phase::kContent, PortalDetector::Status::kTimeout);
  portal_detector()->CompleteAttempt(r);
}

TEST_F(PortalDetectorTest, ReadCompleteHeader) {
  StartAttempt();

  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(PortalDetector::Result(
                  PortalDetector::Phase::kContent,
                  PortalDetector::Status::kSuccess, kNumAttempts, true))));

  EXPECT_CALL(*http_request(), Stop());
  EXPECT_CALL(*https_request(), Stop());
  PortalDetector::Result r = PortalDetector::Result(
      PortalDetector::Phase::kContent, PortalDetector::Status::kSuccess);
  portal_detector()->CompleteAttempt(r);
}

TEST_F(PortalDetectorTest, RequestSuccess) {
  StartAttempt();

  // HTTPS probe does not trigger anything (for now)
  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(PortalDetector::Result(
                  PortalDetector::Phase::kContent,
                  PortalDetector::Status::kSuccess, kNumAttempts, true))))
      .Times(0);
  EXPECT_CALL(*http_request(), Stop()).Times(0);
  EXPECT_CALL(*https_request(), Stop()).Times(0);
  ExpectRequestSuccessWithStatus(204, false);

  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(PortalDetector::Result(
                  PortalDetector::Phase::kContent,
                  PortalDetector::Status::kSuccess, kNumAttempts, true))));
  EXPECT_CALL(*http_request(), Stop()).Times(2);
  EXPECT_CALL(*https_request(), Stop()).Times(2);
  ExpectRequestSuccessWithStatus(204, true);
}

TEST_F(PortalDetectorTest, RequestFail) {
  StartAttempt();

  // HTTPS probe does not trigger anything (for now)
  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(PortalDetector::Result(
                  PortalDetector::Phase::kContent,
                  PortalDetector::Status::kFailure, kNumAttempts, false))))
      .Times(0);
  EXPECT_CALL(*http_request(), Stop()).Times(0);
  EXPECT_CALL(*https_request(), Stop()).Times(0);
  EXPECT_CALL(dispatcher(),
              PostDelayedTask(
                  _, _, PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000))
      .Times(0);
  ExpectRequestSuccessWithStatus(123, false);

  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(PortalDetector::Result(
                  PortalDetector::Phase::kContent,
                  PortalDetector::Status::kFailure, kNumAttempts, false))));
  EXPECT_CALL(*http_request(), Stop()).Times(2);
  EXPECT_CALL(*https_request(), Stop()).Times(2);
  EXPECT_CALL(dispatcher(),
              PostDelayedTask(
                  _, _, PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000));
  ExpectRequestSuccessWithStatus(123, true);
}

TEST_F(PortalDetectorTest, StartSingleTrial) {
  StartSingleTrial();

  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HttpRequest::kResultDNSFailure));
  EXPECT_CALL(*http_request(), Stop()).Times(2);
  EXPECT_CALL(*https_request(), Stop()).Times(2);
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, _, _)).Times(0);
  portal_detector()->StartTrialTask();
}

struct ResultMapping {
  ResultMapping() : http_result(HttpRequest::kResultUnknown), portal_result() {}
  ResultMapping(HttpRequest::Result in_http_result,
                const PortalDetector::Result& in_portal_result)
      : http_result(in_http_result), portal_result(in_portal_result) {}
  HttpRequest::Result http_result;
  PortalDetector::Result portal_result;
};

class PortalDetectorResultMappingTest
    : public testing::TestWithParam<ResultMapping> {};

TEST_P(PortalDetectorResultMappingTest, MapResult) {
  PortalDetector::Result trial_result =
      PortalDetector::GetPortalResultForRequestResult(GetParam().http_result);
  EXPECT_EQ(trial_result.phase, GetParam().portal_result.phase);
  EXPECT_EQ(trial_result.status, GetParam().portal_result.status);
}

INSTANTIATE_TEST_CASE_P(
    TrialResultMappingTest,
    PortalDetectorResultMappingTest,
    ::testing::Values(
        ResultMapping(HttpRequest::kResultUnknown,
                      PortalDetector::Result(PortalDetector::Phase::kUnknown,
                                             PortalDetector::Status::kFailure)),
        ResultMapping(HttpRequest::kResultInvalidInput,
                      PortalDetector::Result(PortalDetector::Phase::kUnknown,
                                             PortalDetector::Status::kFailure)),
        ResultMapping(HttpRequest::kResultInProgress,
                      PortalDetector::Result(PortalDetector::Phase::kUnknown,
                                             PortalDetector::Status::kFailure)),
        ResultMapping(HttpRequest::kResultDNSFailure,
                      PortalDetector::Result(PortalDetector::Phase::kDNS,
                                             PortalDetector::Status::kFailure)),
        ResultMapping(HttpRequest::kResultDNSTimeout,
                      PortalDetector::Result(PortalDetector::Phase::kDNS,
                                             PortalDetector::Status::kTimeout)),
        ResultMapping(HttpRequest::kResultConnectionFailure,
                      PortalDetector::Result(PortalDetector::Phase::kConnection,
                                             PortalDetector::Status::kFailure)),
        ResultMapping(HttpRequest::kResultHTTPFailure,
                      PortalDetector::Result(PortalDetector::Phase::kHTTP,
                                             PortalDetector::Status::kFailure)),
        ResultMapping(HttpRequest::kResultHTTPTimeout,
                      PortalDetector::Result(PortalDetector::Phase::kHTTP,
                                             PortalDetector::Status::kTimeout)),
        ResultMapping(
            HttpRequest::kResultSuccess,
            PortalDetector::Result(PortalDetector::Phase::kContent,
                                   PortalDetector::Status::kFailure))));

}  // namespace shill
