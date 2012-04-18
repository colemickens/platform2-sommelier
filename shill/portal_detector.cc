// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/portal_detector.h"

#include <string>

#include <base/bind.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>

#include "shill/async_connection.h"
#include "shill/connection.h"
#include "shill/dns_client.h"
#include "shill/event_dispatcher.h"
#include "shill/http_url.h"
#include "shill/ip_address.h"
#include "shill/scope_logger.h"
#include "shill/sockets.h"

using base::Bind;
using base::Callback;
using base::StringPrintf;
using std::string;

namespace shill {

const int PortalDetector::kDefaultCheckIntervalSeconds = 30;
const char PortalDetector::kDefaultCheckPortalList[] = "ethernet,wifi,cellular";
const char PortalDetector::kDefaultURL[] =
    "http://clients3.google.com/generate_204";
const char PortalDetector::kResponseExpected[] = "HTTP/1.1 204";

const int PortalDetector::kMaxRequestAttempts = 3;
const int PortalDetector::kMinTimeBetweenAttemptsSeconds = 3;
const int PortalDetector::kRequestTimeoutSeconds = 10;

const char PortalDetector::kPhaseConnectionString[] = "Connection";
const char PortalDetector::kPhaseDNSString[] = "DNS";
const char PortalDetector::kPhaseHTTPString[] = "HTTP";
const char PortalDetector::kPhaseContentString[] = "Content";
const char PortalDetector::kPhaseUnknownString[] = "Unknown";

const char PortalDetector::kStatusFailureString[] = "Failure";
const char PortalDetector::kStatusSuccessString[] = "Success";
const char PortalDetector::kStatusTimeoutString[] = "Timeout";

PortalDetector::PortalDetector(
    ConnectionRefPtr connection,
    EventDispatcher *dispatcher,
    const Callback<void(const Result &)> &callback)
    : attempt_count_(0),
      connection_(connection),
      dispatcher_(dispatcher),
      weak_ptr_factory_(this),
      portal_result_callback_(callback),
      request_read_callback_(
          Bind(&PortalDetector::RequestReadCallback,
               weak_ptr_factory_.GetWeakPtr())),
      request_result_callback_(
          Bind(&PortalDetector::RequestResultCallback,
               weak_ptr_factory_.GetWeakPtr())),
      time_(Time::GetInstance()) { }

PortalDetector::~PortalDetector() {
  Stop();
}

bool PortalDetector::Start(const string &url_string) {
  return StartAfterDelay(url_string, 0);
}

bool PortalDetector::StartAfterDelay(const string &url_string,
                                     int delay_seconds) {
  SLOG(Portal, 3) << "In " << __func__;

  DCHECK(!request_.get());

  if (!url_.ParseFromString(url_string)) {
    LOG(ERROR) << "Failed to parse URL string: " << url_string;
    return false;
  }

  request_.reset(new HTTPRequest(connection_, dispatcher_, &sockets_));
  attempt_count_ = 0;
  StartAttempt(delay_seconds);
  return true;
}

void PortalDetector::Stop() {
  SLOG(Portal, 3) << "In " << __func__;

  if (!request_.get()) {
    return;
  }

  start_attempt_.Cancel();
  StopAttempt();
  attempt_count_ = 0;
  request_.reset();
}

bool PortalDetector::IsInProgress() {
  return attempt_count_ != 0;
}

// static
const string PortalDetector::PhaseToString(Phase phase) {
  switch (phase) {
    case kPhaseConnection:
      return kPhaseConnectionString;
    case kPhaseDNS:
      return kPhaseDNSString;
    case kPhaseHTTP:
      return kPhaseHTTPString;
    case kPhaseContent:
      return kPhaseContentString;
    case kPhaseUnknown:
    default:
      return kPhaseUnknownString;
  }
}

// static
const string PortalDetector::StatusToString(Status status) {
  switch (status) {
    case kStatusSuccess:
      return kStatusSuccessString;
    case kStatusTimeout:
      return kStatusTimeoutString;
    case kStatusFailure:
    default:
      return kStatusFailureString;
  }
}

void PortalDetector::CompleteAttempt(Result result) {
  LOG(INFO) << StringPrintf("Portal detection completed attempt %d with "
                            "phase==%s, status==%s",
                            attempt_count_,
                            PhaseToString(result.phase).c_str(),
                            StatusToString(result.status).c_str());
  StopAttempt();
  if (result.status != kStatusSuccess && attempt_count_ < kMaxRequestAttempts) {
    StartAttempt(0);
  } else {
    result.num_attempts = attempt_count_;
    result.final = true;
    Stop();
  }

  portal_result_callback_.Run(result);
}

PortalDetector::Result PortalDetector::GetPortalResultForRequestResult(
  HTTPRequest::Result result) {
  switch (result) {
    case HTTPRequest::kResultSuccess:
      // The request completed without receiving the expected payload.
      return Result(kPhaseContent, kStatusFailure);
    case HTTPRequest::kResultDNSFailure:
      return Result(kPhaseDNS, kStatusFailure);
    case HTTPRequest::kResultDNSTimeout:
      return Result(kPhaseDNS, kStatusTimeout);
    case HTTPRequest::kResultConnectionFailure:
      return Result(kPhaseConnection, kStatusFailure);
    case HTTPRequest::kResultConnectionTimeout:
      return Result(kPhaseConnection, kStatusTimeout);
    case HTTPRequest::kResultRequestFailure:
    case HTTPRequest::kResultResponseFailure:
      return Result(kPhaseHTTP, kStatusFailure);
    case HTTPRequest::kResultRequestTimeout:
    case HTTPRequest::kResultResponseTimeout:
      return Result(kPhaseHTTP, kStatusTimeout);
    case HTTPRequest::kResultUnknown:
    default:
      return Result(kPhaseUnknown, kStatusFailure);
  }
}

void PortalDetector::RequestReadCallback(const ByteString &response_data) {
  const string response_expected(kResponseExpected);
  bool expected_length_received = false;
  int compare_length = 0;
  if (response_data.GetLength() < response_expected.length()) {
    // There isn't enough data yet for a final decision, but we can still
    // test to see if the partial string matches so far.
    expected_length_received = false;
    compare_length = response_data.GetLength();
  } else {
    expected_length_received = true;
    compare_length = response_expected.length();
  }

  if (ByteString(response_expected.substr(0, compare_length), false).Equals(
          ByteString(response_data.GetConstData(), compare_length))) {
    if (expected_length_received) {
      CompleteAttempt(Result(kPhaseContent, kStatusSuccess));
    }
    // Otherwise, we wait for more data from the server.
  } else {
    CompleteAttempt(Result(kPhaseContent, kStatusFailure));
  }
}

void PortalDetector::RequestResultCallback(
    HTTPRequest::Result result, const ByteString &/*response_data*/) {
  CompleteAttempt(GetPortalResultForRequestResult(result));
}

void PortalDetector::StartAttempt(int init_delay_seconds) {
  int64 next_attempt_delay = 0;
  if (attempt_count_ > 0) {
    // Ensure that attempts are spaced at least by a minimal interval.
    struct timeval now, elapsed_time;
    time_->GetTimeMonotonic(&now);
    timersub(&now, &attempt_start_time_, &elapsed_time);

    if (elapsed_time.tv_sec < kMinTimeBetweenAttemptsSeconds) {
      struct timeval remaining_time = { kMinTimeBetweenAttemptsSeconds, 0 };
      timersub(&remaining_time, &elapsed_time, &remaining_time);
      next_attempt_delay =
        remaining_time.tv_sec * 1000 + remaining_time.tv_usec / 1000;
    }
  } else {
    next_attempt_delay = init_delay_seconds * 1000;
  }

  start_attempt_.Reset(Bind(&PortalDetector::StartAttemptTask,
                            weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostDelayedTask(start_attempt_.callback(), next_attempt_delay);
}

void PortalDetector::StartAttemptTask() {
  time_->GetTimeMonotonic(&attempt_start_time_);
  ++attempt_count_;

  LOG(INFO) << StringPrintf("Portal detection starting attempt %d of %d",
                            attempt_count_, kMaxRequestAttempts);

  HTTPRequest::Result result =
      request_->Start(url_, request_read_callback_, request_result_callback_);
  if (result != HTTPRequest::kResultInProgress) {
    CompleteAttempt(GetPortalResultForRequestResult(result));
    return;
  }

  attempt_timeout_.Reset(Bind(&PortalDetector::TimeoutAttemptTask,
                              weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostDelayedTask(attempt_timeout_.callback(),
                               kRequestTimeoutSeconds * 1000);
}

void PortalDetector::StopAttempt() {
  request_->Stop();
  attempt_timeout_.Cancel();
}

void PortalDetector::TimeoutAttemptTask() {
  LOG(ERROR) << "Request timed out";
  if (request_->response_data().GetLength()) {
    CompleteAttempt(Result(kPhaseContent, kStatusTimeout));
  } else {
    CompleteAttempt(Result(kPhaseUnknown, kStatusTimeout));
  }
}

}  // namespace shill
