// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PORTAL_DETECTOR_
#define SHILL_PORTAL_DETECTOR_

#include <string>
#include <vector>

#include <base/callback_old.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/http_request.h"
#include "shill/http_url.h"
#include "shill/refptr_types.h"
#include "shill/shill_time.h"
#include "shill/sockets.h"

namespace shill {

class ByteString;
class EventDispatcher;
class PortalDetector;
class Time;

// The PortalDetector class implements the portal detection
// facility in shill, which is responsible for checking to see
// if a connection has "general internet connectivity".
//
// This information can be used for ranking one connection
// against another, or for informing UI and other components
// outside the connection manager whether the connection seems
// available for "general use" or if further user action may be
// necessary (e.g, click through of a WiFi Hotspot's splash
// page).
//
// This is achieved by trying to access a URL and expecting a
// specific response.  Any result that deviates from this result
// (DNS or HTTP errors, as well as deviations from the expected
// content) are considered failures.
class PortalDetector {
 public:
  enum Phase {
    kPhaseConnection,
    kPhaseDNS,
    kPhaseHTTP,
    kPhaseContent,
    kPhaseUnknown
  };

  enum Status {
    kStatusFailure,
    kStatusSuccess,
    kStatusTimeout
  };

  struct Result {
    Result() : phase(kPhaseUnknown), status(kStatusFailure), final(false) {}
    Result(Phase phase_in, Status status_in)
        : phase(phase_in), status(status_in), final(false) {}
    Result(Phase phase_in, Status status_in, bool final_in)
        : phase(phase_in), status(status_in), final(final_in) {}
    Phase phase;
    Status status;
    bool final;
  };

  static const int kDefaultCheckIntervalSeconds;
  static const char kDefaultURL[];
  static const char kResponseExpected[];

  PortalDetector(ConnectionRefPtr connection,
                 EventDispatcher *dispatcher,
                 Callback1<const Result&>::Type *callback);
  virtual ~PortalDetector();

  // Start a portal detection test.  Returns true if |url_string| correctly
  // parses as a URL.  Returns false (and does not start) if the |url_string|
  // fails to parse.
  //
  // As each attempt completes the callback handed to the constructor will
  // be called.  The PortalDetector will try up to kMaxRequestAttempts times
  // to successfully retrieve the URL.  If the attempt is successful or
  // this is the last attempt, the "final" flag in the Result structure will
  // be true, otherwise it will be false, and the PortalDetector will
  // schedule the next attempt.
  virtual bool Start(const std::string &url_string);
  virtual bool StartAfterDelay(const std::string &url_string,
                               int delay_seconds);

  // End the current portal detection process if one exists, and do not call
  // the callback.
  virtual void Stop();

  // Returns whether portal request is "in progress": whether the portal
  // detector is in the progress of making attempts.  Returns true if
  // attempts are in progress, false otherwise.  Notably, this function
  // returns false during the period of time between calling "Start" or
  // "StartAfterDelay" and the actual start of the first attempt.
  virtual bool IsInProgress();

  static const std::string PhaseToString(Phase phase);
  static const std::string StatusToString(Status status);
  static Result GetPortalResultForRequestResult(HTTPRequest::Result result);

 private:
  friend class PortalDetectorTest;
  FRIEND_TEST(PortalDetectorTest, StartAttemptFailed);
  FRIEND_TEST(PortalDetectorTest, StartAttemptRepeated);
  FRIEND_TEST(PortalDetectorTest, StartAttemptAfterDelay);
  FRIEND_TEST(PortalDetectorTest, AttemptCount);

  // Number of times to attempt connection.
  static const int kMaxRequestAttempts;
  // Minimum time between attempts to connect to server.
  static const int kMinTimeBetweenAttemptsSeconds;
  // Time to wait for request to complete.
  static const int kRequestTimeoutSeconds;

  static const char kPhaseConnectionString[];
  static const char kPhaseDNSString[];
  static const char kPhaseHTTPString[];
  static const char kPhaseContentString[];
  static const char kPhaseUnknownString[];

  static const char kStatusFailureString[];
  static const char kStatusSuccessString[];
  static const char kStatusTimeoutString[];

  void CompleteAttempt(Result result);
  void RequestReadCallback(const ByteString &response_data);
  void RequestResultCallback(HTTPRequest::Result result,
                             const ByteString &response_data);
  void StartAttempt(int init_delay_seconds);
  void StartAttemptTask();
  void StopAttempt();
  void TimeoutAttemptTask();

  int attempt_count_;
  struct timeval attempt_start_time_;
  ConnectionRefPtr connection_;
  EventDispatcher *dispatcher_;
  Callback1<const Result &>::Type *portal_result_callback_;
  scoped_ptr<HTTPRequest> request_;
  scoped_ptr<Callback1<const ByteString &>::Type> request_read_callback_;
  scoped_ptr<Callback2<HTTPRequest::Result, const ByteString &>::Type>
      request_result_callback_;
  Sockets sockets_;
  ScopedRunnableMethodFactory<PortalDetector> task_factory_;
  Time *time_;
  HTTPURL url_;

  DISALLOW_COPY_AND_ASSIGN(PortalDetector);
};

}  // namespace shill

#endif  // SHILL_PORTAL_DETECTOR_
