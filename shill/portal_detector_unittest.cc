// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/portal_detector.h"

#include <string>

#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_connection.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_http_request.h"
#include "shill/mock_time.h"

using std::string;
using std::vector;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {
const char kBadURL[] = "badurl";
const char kInterfaceName[] = "int0";
const char kURL[] = "http://www.chromium.org";
const char kDNSServer0[] = "8.8.8.8";
const char kDNSServer1[] = "8.8.4.4";
const char *kDNSServers[] = { kDNSServer0, kDNSServer1 };
}  // namespace {}

MATCHER_P(IsResult, result, "") {
  return (result.phase == arg.phase &&
          result.status == arg.status &&
          result.final == arg.final);
}

class PortalDetectorTest : public Test {
 public:
  PortalDetectorTest()
      : device_info_(new NiceMock<MockDeviceInfo>(
          &control_,
          reinterpret_cast<EventDispatcher *>(NULL),
          reinterpret_cast<Metrics *>(NULL),
          reinterpret_cast<Manager *>(NULL))),
        connection_(new StrictMock<MockConnection>(device_info_.get())),
        portal_detector_(new PortalDetector(
            connection_.get(),
            &dispatcher_,
            callback_target_.result_callback())),
        interface_name_(kInterfaceName),
        dns_servers_(kDNSServers, kDNSServers + 2),
        http_request_(NULL) {
    current_time_.tv_sec = current_time_.tv_usec = 0;
  }

  virtual void SetUp() {
    EXPECT_CALL(*connection_.get(), interface_name())
        .WillRepeatedly(ReturnRef(interface_name_));
    portal_detector_->time_ = &time_;
    EXPECT_CALL(time_, GetTimeMonotonic(_))
        .WillRepeatedly(Invoke(this, &PortalDetectorTest::GetTimeMonotonic));
    EXPECT_CALL(*connection_.get(), dns_servers())
        .WillRepeatedly(ReturnRef(dns_servers_));
  }

  virtual void TearDown() {
    if (portal_detector_->request_.get()) {
      EXPECT_CALL(*http_request(), Stop());

      // Delete the portal detector while expectations still exist.
      portal_detector_.reset();
    }
  }

 protected:
  class CallbackTarget {
   public:
    CallbackTarget()
        : result_callback_(NewCallback(this, &CallbackTarget::ResultCallback)) {
    }

    MOCK_METHOD1(ResultCallback, void(const PortalDetector::Result &result));
    Callback1<const PortalDetector::Result &>::Type *result_callback() {
      return result_callback_.get();
    }

   private:
    scoped_ptr<Callback1<const PortalDetector::Result &>::Type>
        result_callback_;
  };

  void AssignHTTPRequest() {
    http_request_ = new StrictMock<MockHTTPRequest>(connection_);
    portal_detector_->request_.reset(http_request_);  // Passes ownership.
  }

  bool StartPortalRequest(const string &url_string) {
    bool ret = portal_detector_->Start(url_string);
    if (ret) {
      AssignHTTPRequest();
    }
    return ret;
  }

  void TimeoutAttempt() {
    portal_detector_->TimeoutAttemptTask();
  }

  MockHTTPRequest *http_request() { return http_request_; }
  PortalDetector *portal_detector() { return portal_detector_.get(); }
  MockEventDispatcher &dispatcher() { return dispatcher_; }
  CallbackTarget &callback_target() { return callback_target_; }
  ByteString &response_data() { return response_data_; }

  void ExpectReset() {
    EXPECT_FALSE(portal_detector_->attempt_count_);
    EXPECT_EQ(callback_target_.result_callback(),
              portal_detector_->portal_result_callback_);
    EXPECT_FALSE(portal_detector_->request_.get());
  }

  void ExpectAttemptRetry(const PortalDetector::Result &result) {
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(result)));

    // Expect the PortalDetector to stop the current request.
    EXPECT_CALL(*http_request(), Stop());

    // Expect the PortalDetector to schedule the next attempt.
    EXPECT_CALL(
        dispatcher(),
        PostDelayedTask(
            _, PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000));
  }

  void AdvanceTime(int milliseconds) {
    struct timeval tv = { milliseconds / 1000, (milliseconds % 1000) * 1000 };
    timeradd(&current_time_, &tv, &current_time_);
  }

  void StartAttempt() {
    EXPECT_CALL(dispatcher(), PostDelayedTask(_, 0));
    EXPECT_TRUE(StartPortalRequest(kURL));

    // Expect that the request will be started -- return failure.
    EXPECT_CALL(*http_request(), Start(_, _, _))
        .WillOnce(Return(HTTPRequest::kResultInProgress));
    EXPECT_CALL(dispatcher(), PostDelayedTask(
        _, PortalDetector::kRequestTimeoutSeconds * 1000));

    portal_detector()->StartAttemptTask();
  }

  void AppendReadData(const string &read_data) {
    response_data_.Append(ByteString(read_data, false));
    EXPECT_CALL(*http_request_, response_data())
        .WillOnce(ReturnRef(response_data_));
    portal_detector_->RequestReadCallback(response_data_.GetLength());
  }

 private:
  int GetTimeMonotonic(struct timeval *tv) {
    *tv = current_time_;
    return 0;
  }

  StrictMock<MockEventDispatcher> dispatcher_;
  MockControl control_;
  scoped_ptr<MockDeviceInfo> device_info_;
  scoped_refptr<MockConnection> connection_;
  CallbackTarget callback_target_;
  scoped_ptr<PortalDetector> portal_detector_;
  StrictMock<MockTime> time_;
  struct timeval current_time_;
  const string interface_name_;
  vector<string> dns_servers_;
  ByteString response_data_;

  // Owned by the PortalDetector, but tracked here for EXPECT_CALL()
  MockHTTPRequest *http_request_;
};

TEST_F(PortalDetectorTest, Constructor) {
  ExpectReset();
}

TEST_F(PortalDetectorTest, InvalidURL) {
  EXPECT_FALSE(StartPortalRequest(kBadURL));
  ExpectReset();
}

TEST_F(PortalDetectorTest, StartAttemptFailed) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, 0));
  EXPECT_TRUE(StartPortalRequest(kURL));

  // Expect that the request will be started -- return failure.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HTTPRequest::kResultConnectionFailure));
  // Expect a non-final failure to be relayed to the caller.
  ExpectAttemptRetry(PortalDetector::Result(
      PortalDetector::kPhaseConnection,
      PortalDetector::kStatusFailure,
      false));
  portal_detector()->StartAttemptTask();
}

TEST_F(PortalDetectorTest, StartAttemptRepeated) {
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, 0));
  portal_detector()->StartAttempt();

  AssignHTTPRequest();
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .WillOnce(Return(HTTPRequest::kResultInProgress));
  EXPECT_CALL(
      dispatcher(),
      PostDelayedTask(
          _, PortalDetector::kRequestTimeoutSeconds * 1000));
  portal_detector()->StartAttemptTask();

  // A second attempt should be delayed by kMinTimeBetweenAttemptsSeconds.
  EXPECT_CALL(
      dispatcher(),
      PostDelayedTask(
          _, PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000));
  portal_detector()->StartAttempt();
}

TEST_F(PortalDetectorTest, AttemptCount) {
  // Expect the PortalDetector to immediately post a task for the each attempt.
  EXPECT_CALL(dispatcher(), PostDelayedTask(_, 0))
      .Times(PortalDetector::kMaxRequestAttempts);
  EXPECT_TRUE(StartPortalRequest(kURL));

  // Expect that the request will be started -- return failure.
  EXPECT_CALL(*http_request(), Start(_, _, _))
      .Times(PortalDetector::kMaxRequestAttempts)
      .WillRepeatedly(Return(HTTPRequest::kResultInProgress));

  // Each HTTP request that gets started will have a request timeout.
  EXPECT_CALL(dispatcher(), PostDelayedTask(
      _, PortalDetector::kRequestTimeoutSeconds * 1000))
      .Times(PortalDetector::kMaxRequestAttempts);

  {
    InSequence s;

    // Expect non-final failures for all attempts but the last.
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(
                    PortalDetector::Result(
                        PortalDetector::kPhaseDNS,
                        PortalDetector::kStatusFailure,
                        false))))
        .Times(PortalDetector::kMaxRequestAttempts - 1);

    // Expect a single final failure.
    EXPECT_CALL(callback_target(),
                ResultCallback(IsResult(
                    PortalDetector::Result(
                        PortalDetector::kPhaseDNS,
                        PortalDetector::kStatusFailure,
                        true))))
        .Times(1);
  }

  // Expect the PortalDetector to stop the current request each time, plus
  // an extra time in PortalDetector::Stop().
  EXPECT_CALL(*http_request(), Stop())
      .Times(PortalDetector::kMaxRequestAttempts + 1);


  for (int i = 0; i < PortalDetector::kMaxRequestAttempts; i++) {
    portal_detector()->StartAttemptTask();
    AdvanceTime(PortalDetector::kMinTimeBetweenAttemptsSeconds * 1000);
    portal_detector()->RequestResultCallback(HTTPRequest::kResultDNSFailure);
  }

  ExpectReset();
}

TEST_F(PortalDetectorTest, ReadBadHeader) {
  StartAttempt();

  ExpectAttemptRetry(PortalDetector::Result(
      PortalDetector::kPhaseContent,
      PortalDetector::kStatusFailure,
      false));
  AppendReadData("X");
}

TEST_F(PortalDetectorTest, RequestTimeout) {
  StartAttempt();
  ExpectAttemptRetry(PortalDetector::Result(
      PortalDetector::kPhaseUnknown,
      PortalDetector::kStatusTimeout,
      false));

  EXPECT_CALL(*http_request(), response_data())
      .WillOnce(ReturnRef(response_data()));

  TimeoutAttempt();
}

TEST_F(PortalDetectorTest, ReadPartialHeaderTimeout) {
  StartAttempt();

  const string response_expected(PortalDetector::kResponseExpected);
  const size_t partial_size = response_expected.length() / 2;
  AppendReadData(response_expected.substr(0, partial_size));

  ExpectAttemptRetry(PortalDetector::Result(
      PortalDetector::kPhaseContent,
      PortalDetector::kStatusTimeout,
      false));

  EXPECT_CALL(*http_request(), response_data())
      .WillOnce(ReturnRef(response_data()));

  TimeoutAttempt();
}

TEST_F(PortalDetectorTest, ReadCompleteHeader) {
  const string response_expected(PortalDetector::kResponseExpected);
  const size_t partial_size = response_expected.length() / 2;

  StartAttempt();
  AppendReadData(response_expected.substr(0, partial_size));

  EXPECT_CALL(callback_target(),
              ResultCallback(IsResult(
                  PortalDetector::Result(
                      PortalDetector::kPhaseContent,
                      PortalDetector::kStatusSuccess,
                      true))));
  EXPECT_CALL(*http_request(), Stop())
      .Times(2);

  AppendReadData(response_expected.substr(partial_size));
}
struct ResultMapping {
  ResultMapping() : http_result(HTTPRequest::kResultUnknown), portal_result() {}
  ResultMapping(HTTPRequest::Result in_http_result,
                const PortalDetector::Result &in_portal_result)
      : http_result(in_http_result),
        portal_result(in_portal_result) {}
  HTTPRequest::Result http_result;
  PortalDetector::Result portal_result;
};

class PortalDetectorResultMappingTest
    : public testing::TestWithParam<ResultMapping> {};

TEST_P(PortalDetectorResultMappingTest, MapResult) {
  PortalDetector::Result portal_result =
      PortalDetector::GetPortalResultForRequestResult(GetParam().http_result);
  EXPECT_EQ(portal_result.phase, GetParam().portal_result.phase);
  EXPECT_EQ(portal_result.status, GetParam().portal_result.status);
}

INSTANTIATE_TEST_CASE_P(
    PortalResultMappingTest,
    PortalDetectorResultMappingTest,
    ::testing::Values(
        ResultMapping(HTTPRequest::kResultUnknown,
                      PortalDetector::Result(PortalDetector::kPhaseUnknown,
                                             PortalDetector::kStatusFailure)),
        ResultMapping(HTTPRequest::kResultInProgress,
                      PortalDetector::Result(PortalDetector::kPhaseUnknown,
                                             PortalDetector::kStatusFailure)),
        ResultMapping(HTTPRequest::kResultDNSFailure,
                      PortalDetector::Result(PortalDetector::kPhaseDNS,
                                             PortalDetector::kStatusFailure)),
        ResultMapping(HTTPRequest::kResultDNSTimeout,
                      PortalDetector::Result(PortalDetector::kPhaseDNS,
                                             PortalDetector::kStatusTimeout)),
        ResultMapping(HTTPRequest::kResultConnectionFailure,
                      PortalDetector::Result(PortalDetector::kPhaseConnection,
                                             PortalDetector::kStatusFailure)),
        ResultMapping(HTTPRequest::kResultConnectionTimeout,
                      PortalDetector::Result(PortalDetector::kPhaseConnection,
                                             PortalDetector::kStatusTimeout)),
        ResultMapping(HTTPRequest::kResultRequestFailure,
                      PortalDetector::Result(PortalDetector::kPhaseHTTP,
                                             PortalDetector::kStatusFailure)),
        ResultMapping(HTTPRequest::kResultRequestTimeout,
                      PortalDetector::Result(PortalDetector::kPhaseHTTP,
                                             PortalDetector::kStatusTimeout)),
        ResultMapping(HTTPRequest::kResultResponseFailure,
                      PortalDetector::Result(PortalDetector::kPhaseHTTP,
                                             PortalDetector::kStatusFailure)),
        ResultMapping(HTTPRequest::kResultResponseTimeout,
                      PortalDetector::Result(PortalDetector::kPhaseHTTP,
                                             PortalDetector::kStatusTimeout)),
        ResultMapping(HTTPRequest::kResultSuccess,
                      PortalDetector::Result(PortalDetector::kPhaseContent,
                                             PortalDetector::kStatusFailure))));

}  // namespace shill
