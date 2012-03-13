// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DNS_CLIENT_
#define SHILL_DNS_CLIENT_

#include <string>
#include <vector>

#include <base/callback_old.h>
#include <base/memory/scoped_ptr.h>
#include <base/task.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ip_address.h"
#include "shill/refptr_types.h"

struct hostent;

namespace shill {

class Ares;
class Time;
struct DNSClientState;

// Implements a DNS resolution client that can run asynchronously.
class DNSClient {
 public:
  typedef Callback2<const Error &, const IPAddress &>::Type ClientCallback;

  static const int kDefaultTimeoutMS;
  static const char kErrorNoData[];
  static const char kErrorFormErr[];
  static const char kErrorServerFail[];
  static const char kErrorNotFound[];
  static const char kErrorNotImp[];
  static const char kErrorRefused[];
  static const char kErrorBadQuery[];
  static const char kErrorNetRefused[];
  static const char kErrorTimedOut[];
  static const char kErrorUnknown[];

  DNSClient(IPAddress::Family family,
            const std::string &interface_name,
            const std::vector<std::string> &dns_servers,
            int timeout_ms,
            EventDispatcher *dispatcher,
            ClientCallback *callback);
  virtual ~DNSClient();

  // Returns true if the DNS client started successfully, false otherwise.
  // If successful, the callback will be called with the result of the
  // request.  If Start() fails and returns false, the callback will not
  // be called, but the error that caused the failure will be returned in
  // |error|.
  virtual bool Start(const std::string &hostname, Error *error);

  // Aborts any running DNS client transaction.  This will cancel any callback
  // invocation.
  virtual void Stop();

 private:
  friend class DNSClientTest;

  void HandleCompletion();
  void HandleDNSRead(int fd);
  void HandleDNSWrite(int fd);
  void HandleTimeout();
  void ReceiveDNSReply(int status, struct hostent *hostent);
  static void ReceiveDNSReplyCB(void *arg, int status, int timeouts,
                                struct hostent *hostent);
  bool RefreshHandles();

  Error error_;
  IPAddress address_;
  std::string interface_name_;
  std::vector<std::string> dns_servers_;
  EventDispatcher *dispatcher_;
  ClientCallback *callback_;
  int timeout_ms_;
  bool running_;
  scoped_ptr<DNSClientState> resolver_state_;
  scoped_ptr<Callback1<int>::Type> read_callback_;
  scoped_ptr<Callback1<int>::Type> write_callback_;
  ScopedRunnableMethodFactory<DNSClient> task_factory_;
  Ares *ares_;
  Time *time_;

  DISALLOW_COPY_AND_ASSIGN(DNSClient);
};

}  // namespace shill

#endif  // SHILL_DNS_CLIENT_
