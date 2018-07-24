// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DNS_SERVER_TESTER_H_
#define SHILL_DNS_SERVER_TESTER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/net/ip_address.h"
#include "shill/refptr_types.h"

namespace shill {

class DNSClient;
class Error;
class EventDispatcher;

// The DNSServerTester class implements the DNS health check
// facility in shill, which is responsible for checking to see
// if the given DNS servers are working or not.
//
// The tester support two modes of operation, continuous and
// non-continuous mode. With continuous mode (retry_until_success_ flag is set),
// the tester will continue to perform DNS test until the DNS test succeed or
// the DNS client failed to start. The callback is only invoke when the test
// succeed or we failed to start the DNS client. With non-continuous mode,
// only one DNS test is performed. And the callback is invoked regardless of
// the result of the test.
class DNSServerTester {
 public:
  enum Status {
    kStatusFailure,
    kStatusSuccess,
  };

  DNSServerTester(ConnectionRefPtr connection,
                  EventDispatcher* dispatcher,
                  const std::vector<std::string>& dns_servers,
                  const bool retry_until_success,
                  const base::Callback<void(const Status)>& callback);
  virtual ~DNSServerTester();

  // Start the test.
  virtual void Start();

  // End the current DNS test process if one exist, and do not call
  // the callback.
  virtual void Stop();

 private:
  friend class DNSServerTesterTest;
  FRIEND_TEST(DNSServerTesterTest, StartAttempt);
  FRIEND_TEST(DNSServerTesterTest, StartAttemptTask);
  FRIEND_TEST(DNSServerTesterTest, AttemptCompleted);
  FRIEND_TEST(DNSServerTesterTest, StopAttempt);

  static const char kDNSTestHostname[];
  static const int kDNSTestRetryIntervalMilliseconds;
  static const int kDNSTimeoutMilliseconds;

  void StartAttempt(int delay_ms);
  void StartAttemptTask();
  void StopAttempt();
  void CompleteAttempt(Status status);
  void DNSClientCallback(const Error& error, const IPAddress& ip);

  ConnectionRefPtr connection_;
  EventDispatcher* dispatcher_;
  // Flag indicating to continuously probing the DNS servers until it succeed.
  // The callback is only invoke when the test succeed or test failed to start.
  bool retry_until_success_;
  base::WeakPtrFactory<DNSServerTester> weak_ptr_factory_;
  base::CancelableClosure start_attempt_;
  base::Callback<void(const Status)> dns_result_callback_;
  base::Callback<void(const Error&, const IPAddress&)>
      dns_client_callback_;
  std::unique_ptr<DNSClient> dns_test_client_;

  DISALLOW_COPY_AND_ASSIGN(DNSServerTester);
};

}  // namespace shill

#endif  // SHILL_DNS_SERVER_TESTER_H_
