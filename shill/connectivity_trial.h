// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONNECTIVITY_TRIAL_H_
#define SHILL_CONNECTIVITY_TRIAL_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <brillo/http/http_request.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/http_request.h"
#include "shill/http_url.h"
#include "shill/net/shill_time.h"
#include "shill/net/sockets.h"
#include "shill/refptr_types.h"

namespace shill {

class EventDispatcher;

// The ConnectivityTrial class implements a single portal detection
// trial.  Each trial checks if a connection has "general internet
// connectivity."
//
// ConnectivityTrial is responsible for managing the callbacks between the
// calling class requesting a connectivity trial and the HttpRequest that is
// used to test connectivity.  ConnectivityTrial maps between the HttpRequest
// response codes to higher-level connection-oriented status.
//
// ConnectivityTrial tests the connection by attempting to parse and access a
// given URL.  Any result that deviates from the expected behavior (DNS or HTTP
// errors, as well as retrieved content errors, and timeouts) are considered
// failures.

class ConnectivityTrial {
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
    Result()
        : phase(kPhaseUnknown), status(kStatusFailure) {}
    Result(Phase phase_in, Status status_in)
        : phase(phase_in), status(status_in) {}
    Phase phase;
    Status status;
  };

  struct PortalDetectionProperties {
    PortalDetectionProperties()
        : http_url_string(kDefaultHttpsUrl),
          https_url_string(kDefaultHttpsUrl) {}
    PortalDetectionProperties(const std::string& http_url_string,
                              const std::string& https_url_string)
        : http_url_string(http_url_string),
          https_url_string(https_url_string) {}
    bool operator==(const PortalDetectionProperties& b) const {
      return http_url_string == b.http_url_string &&
             https_url_string == b.https_url_string;
    }

    std::string http_url_string;
    std::string https_url_string;
  };

  static const char kDefaultHttpUrl[];
  static const char kDefaultHttpsUrl[];

  ConnectivityTrial(ConnectionRefPtr connection,
                    EventDispatcher* dispatcher,
                    int trial_timeout_seconds,
                    const base::Callback<void(Result)>& trial_callback);
  virtual ~ConnectivityTrial();

  // Static method used to map a portal detection phase tp a string.  This
  // includes the phases for connection, DNS, HTTP, returned content and
  // unknown.
  static const std::string PhaseToString(Phase phase);

  // Static method to map from the result of a portal detection phase to a
  // status string. This method supports success, timeout and failure.
  static const std::string StatusToString(Status status);

  // Static method mapping from HttpRequest responses to ConntectivityTrial
  // phases for portal detection. For example, if the HttpRequest result is
  // HttpRequest::kResultDNSFailure, this method returns a
  // ConnectivityTrial::Result with the phase set to
  // ConnectivityTrial::kPhaseDNS and the status set to
  // ConnectivityTrial::kStatusFailure.
  static Result GetPortalResultForRequestResult(HttpRequest::Result result);

  // Start a ConnectivityTrial with the supplied URL and starting delay (ms).
  // Returns trus if |url_string| correctly parses as a URL.  Returns false (and
  // does not start) if the |url_string| fails to parse.
  //
  // After a trial completes, the callback supplied in the constructor is
  // called.
  virtual bool Start(const PortalDetectionProperties& props,
                     int start_delay_milliseconds);

  // After a trial completes, the calling class may call Retry on the trial.
  // This allows the underlying HttpRequest object to be reused.  The URL is not
  // reparsed and the original URL supplied in the Start command is used.  The
  // |start_delay| is the time (ms) to wait before starting the trial.  Retry
  // returns true if the underlying HttpRequest is still available.  If the
  // HttpRequest was reset or never created, Retry will return false.
  virtual bool Retry(int start_delay_milliseconds);

  // End the current attempt if one is in progress.  Will not call the callback
  // with any intermediate results.
  // The ConnectivityTrial will cancel any existing scheduled tasks and destroy
  // the underlying HttpRequest.
  virtual void Stop();

  // Method to return if the connection is being actively tested.
  virtual bool IsActive();

 private:
  friend class PortalDetectorTest;
  FRIEND_TEST(PortalDetectorTest, StartAttemptFailed);
  FRIEND_TEST(PortalDetectorTest, AttemptCount);
  FRIEND_TEST(PortalDetectorTest, ReadBadHeadersRetry);
  FRIEND_TEST(PortalDetectorTest, ReadBadHeader);
  friend class ConnectivityTrialTest;
  FRIEND_TEST(ConnectivityTrialTest, StartAttemptFailed);
  FRIEND_TEST(ConnectivityTrialTest, TrialRetry);
  FRIEND_TEST(ConnectivityTrialTest, ReadBadHeadersRetry);
  FRIEND_TEST(ConnectivityTrialTest, IsActive);

  // Start a ConnectivityTrial with the supplied delay in ms.
  void StartTrialAfterDelay(int start_delay_milliseconds);

  // Internal method used to start the actual connectivity trial, called after
  // the start delay completes.
  void StartTrialTask();

  // Callback used to return data read from the HttpRequest.
  void RequestSuccessCallback(std::shared_ptr<brillo::http::Response> response);

  // Callback used to return the error from the HttpRequest.
  void RequestErrorCallback(HttpRequest::Result result);

  // Internal method used to clean up state and call the original caller that
  // created and triggered this ConnectivityTrial.
  void CompleteTrial(Result result);

  // Internal method used to cancel the timeout timer and stop an active
  // HttpRequest.  If |reset_request| is true, this method resets the underlying
  // HttpRequest object.
  void CleanupTrial(bool reset_request);

  // Callback used to cancel the underlying HttpRequest in the event of a
  // timeout.
  void TimeoutTrialTask();

  ConnectionRefPtr connection_;
  EventDispatcher* dispatcher_;
  int trial_timeout_seconds_;
  base::Callback<void(Result)> trial_callback_;
  base::WeakPtrFactory<ConnectivityTrial> weak_ptr_factory_;
  base::Callback<void(std::shared_ptr<brillo::http::Response>)>
      request_success_callback_;
  base::Callback<void(HttpRequest::Result)> request_error_callback_;
  std::unique_ptr<HttpRequest> http_request_;

  Sockets sockets_;
  std::string http_url_string_;
  base::CancelableClosure trial_;
  base::CancelableClosure trial_timeout_;
  bool is_active_;
};

}  // namespace shill

#endif  // SHILL_CONNECTIVITY_TRIAL_H_
