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

#include "shill/event_dispatcher.h"
#include "shill/ip_address.h"
#include "shill/refptr_types.h"

struct hostent;

namespace shill {

class Ares;
class Time;
struct DNSClientState;

// Implements a DNS resolution client that can run asynchronously
class DNSClient {
 public:
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
            Callback1<bool>::Type *callback);
  ~DNSClient();

  bool Start(const std::string &hostname);
  void Stop();
  const IPAddress &address() const { return address_; }
  const std::string &error() const { return error_; }

 private:
  friend class DNSClientTest;

  void HandleDNSRead(int fd);
  void HandleDNSWrite(int fd);
  void HandleTimeout();
  void ReceiveDNSReply(int status, struct hostent *hostent);
  static void ReceiveDNSReplyCB(void *arg, int status, int timeouts,
                                struct hostent *hostent);
  bool RefreshHandles();

  IPAddress address_;
  std::string interface_name_;
  std::vector<std::string> dns_servers_;
  EventDispatcher *dispatcher_;
  Callback1<bool>::Type *callback_;
  int timeout_ms_;
  bool running_;
  std::string error_;
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
