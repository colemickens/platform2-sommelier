// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connectivity_trial.h"

#include <string>

#include <base/bind.h>
#include <base/rand_util.h>
#include <base/strings/pattern.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/connection.h"
#include "shill/dns_client.h"
#include "shill/event_dispatcher.h"
#include "shill/http_request.h"
#include "shill/http_url.h"
#include "shill/logging.h"
#include "shill/net/ip_address.h"
#include "shill/net/sockets.h"

using base::Bind;
using base::Callback;
using base::StringPrintf;
using std::string;

namespace {

// This keyword gets replaced with a number from the below range.
const char kRandomKeyword[] = "${RAND}";

// This range is determined by the server-side configuration.  See b/63033351
const int kMinRandomHost = 1;
const int kMaxRandomHost = 25;

// If |in| contains the substring |kRandomKeyword|, replace it with a
// random number between |kMinRandomHost| and |kMaxRandomHost| and return
// the newly-mangled string.  Otherwise return an exact copy of |in|.  This
// is used to rotate through alternate hostnames (e.g. alt1..alt25) on
// each portal check, to defeat IP-based blocking.
string RandomizeURL(string url) {
  int alt_host = base::RandInt(kMinRandomHost, kMaxRandomHost);
  base::ReplaceFirstSubstringAfterOffset(
      &url, 0, kRandomKeyword, base::IntToString(alt_host));
  return url;
}

}  // namespace

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kPortal;
static string ObjectID(Connection* c) { return c->interface_name(); }
}

const char ConnectivityTrial::kDefaultHttpUrl[] =
    "http://www.gstatic.com/generate_204";
const char ConnectivityTrial::kDefaultHttpsUrl[] =
    "https://www.google.com/generate_204";

ConnectivityTrial::ConnectivityTrial(ConnectionRefPtr connection,
                                     EventDispatcher* dispatcher,
                                     int trial_timeout_seconds,
                                     const Callback<void(Result)>& callback)
    : connection_(connection),
      dispatcher_(dispatcher),
      trial_timeout_seconds_(trial_timeout_seconds),
      trial_callback_(callback),
      weak_ptr_factory_(this),
      request_success_callback_(Bind(&ConnectivityTrial::RequestSuccessCallback,
                                     weak_ptr_factory_.GetWeakPtr())),
      request_error_callback_(Bind(&ConnectivityTrial::RequestErrorCallback,
                                   weak_ptr_factory_.GetWeakPtr())),
      is_active_(false) {}

ConnectivityTrial::~ConnectivityTrial() {
  Stop();
}

bool ConnectivityTrial::Retry(int start_delay_milliseconds) {
  SLOG(connection_.get(), 3) << "In " << __func__;
  if (http_request_.get())
    CleanupTrial(false);
  else
    return false;
  StartTrialAfterDelay(start_delay_milliseconds);
  return true;
}

bool ConnectivityTrial::Start(const PortalDetectionProperties& props,
                              int start_delay_milliseconds) {
  SLOG(connection_.get(), 3) << "In " << __func__;

  // This step is rerun on each attempt, but trying it here will allow
  // Start() to abort on any obviously malformed URL strings.
  HttpUrl http_url, https_url;
  if (!http_url.ParseFromString(RandomizeURL(props.http_url_string))) {
    LOG(ERROR) << "Failed to parse URL string: " << props.http_url_string;
    return false;
  }
  if (!https_url.ParseFromString(props.https_url_string)) {
    LOG(ERROR) << "Failed to parse URL string: " << props.https_url_string;
    return false;
  }
  http_url_string_ = props.http_url_string;

  if (http_request_.get()) {
    CleanupTrial(false);
  } else {
    http_request_.reset(new HttpRequest(connection_, dispatcher_));
  }
  StartTrialAfterDelay(start_delay_milliseconds);
  return true;
}

void ConnectivityTrial::Stop() {
  SLOG(connection_.get(), 3) << "In " << __func__;

  if (!http_request_.get()) {
    return;
  }

  CleanupTrial(true);
}

void ConnectivityTrial::StartTrialAfterDelay(int start_delay_milliseconds) {
  SLOG(connection_.get(), 4) << "In " << __func__
                             << " delay = " << start_delay_milliseconds
                             << "ms.";
  trial_.Reset(Bind(&ConnectivityTrial::StartTrialTask,
                    weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostDelayedTask(
      FROM_HERE, trial_.callback(), start_delay_milliseconds);
}

void ConnectivityTrial::StartTrialTask() {
  HttpUrl url;
  if (!url.ParseFromString(RandomizeURL(http_url_string_))) {
    LOG(ERROR) << "Failed to parse URL string: " << http_url_string_;
    CompleteTrial(Result(kPhaseUnknown, kStatusFailure));
    return;
  }

  HttpRequest::Result result = http_request_->Start(
      url, request_success_callback_, request_error_callback_);
  if (result != HttpRequest::kResultInProgress) {
    CompleteTrial(ConnectivityTrial::GetPortalResultForRequestResult(result));
    return;
  }
  is_active_ = true;

  trial_timeout_.Reset(Bind(&ConnectivityTrial::TimeoutTrialTask,
                            weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostDelayedTask(FROM_HERE, trial_timeout_.callback(),
                               trial_timeout_seconds_ * 1000);
}

bool ConnectivityTrial::IsActive() {
  return is_active_;
}

void ConnectivityTrial::RequestSuccessCallback(
    std::shared_ptr<brillo::http::Response> response) {
  // TODO(matthewmwang): check for 0 length data as well
  if (response->GetStatusCode() == brillo::http::status_code::NoContent) {
    CompleteTrial(Result(kPhaseContent, kStatusSuccess));
  } else {
    CompleteTrial(Result(kPhaseContent, kStatusFailure));
  }
}

void ConnectivityTrial::RequestErrorCallback(HttpRequest::Result result) {
  CompleteTrial(GetPortalResultForRequestResult(result));
}

void ConnectivityTrial::CompleteTrial(Result result) {
  SLOG(connection_.get(), 3)
      << StringPrintf("Connectivity Trial completed with phase==%s, status==%s",
                      PhaseToString(result.phase).c_str(),
                      StatusToString(result.status).c_str());
  CleanupTrial(false);
  trial_callback_.Run(result);
}

void ConnectivityTrial::CleanupTrial(bool reset_request) {
  trial_timeout_.Cancel();

  if (http_request_.get())
    http_request_->Stop();

  is_active_ = false;

  if (!reset_request || !http_request_.get())
    return;

  http_request_.reset();
}

void ConnectivityTrial::TimeoutTrialTask() {
  LOG(ERROR) << "Connectivity Trial - Request timed out";
  CompleteTrial(ConnectivityTrial::Result(ConnectivityTrial::kPhaseUnknown,
                                          ConnectivityTrial::kStatusTimeout));
}

// statiic
const string ConnectivityTrial::PhaseToString(Phase phase) {
  switch (phase) {
    case kPhaseConnection:
      return kPortalDetectionPhaseConnection;
    case kPhaseDNS:
      return kPortalDetectionPhaseDns;
    case kPhaseHTTP:
      return kPortalDetectionPhaseHttp;
    case kPhaseContent:
      return kPortalDetectionPhaseContent;
    case kPhaseUnknown:
    default:
      return kPortalDetectionPhaseUnknown;
  }
}

// static
const string ConnectivityTrial::StatusToString(Status status) {
  switch (status) {
    case kStatusSuccess:
      return kPortalDetectionStatusSuccess;
    case kStatusTimeout:
      return kPortalDetectionStatusTimeout;
    case kStatusFailure:
    default:
      return kPortalDetectionStatusFailure;
  }
}

ConnectivityTrial::Result ConnectivityTrial::GetPortalResultForRequestResult(
    HttpRequest::Result result) {
  switch (result) {
    case HttpRequest::kResultSuccess:
      // The request completed without receiving the expected payload.
      return Result(kPhaseContent, kStatusFailure);
    case HttpRequest::kResultDNSFailure:
      return Result(kPhaseDNS, kStatusFailure);
    case HttpRequest::kResultDNSTimeout:
      return Result(kPhaseDNS, kStatusTimeout);
    case HttpRequest::kResultConnectionFailure:
      return Result(kPhaseConnection, kStatusFailure);
    case HttpRequest::kResultHTTPFailure:
      return Result(kPhaseHTTP, kStatusFailure);
    case HttpRequest::kResultHTTPTimeout:
      return Result(kPhaseHTTP, kStatusTimeout);
    case HttpRequest::kResultUnknown:
    default:
      return Result(kPhaseUnknown, kStatusFailure);
  }
}

}  // namespace shill
