// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONNECTION_HEALTH_CHECKER_H_
#define SHILL_CONNECTION_HEALTH_CHECKER_H_

#include <queue>

#include <base/basictypes.h>
#include <base/callback.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <base/memory/weak_ptr.h>
#include <gtest/gtest_prod.h>

#include "shill/refptr_types.h"
#include "shill/sockets.h"
#include "shill/socket_info.h"

namespace shill {

class AsyncConnection;
class DNSClient;
class Error;
class EventDispatcher;
class IPAddress;
class SocketInfoReader;

// The ConnectionHealthChecker class implements the facilities to test
// connectivity status on some connection asynchronously.
// In particular, the class can distinguish between three states of the
// connection:
//   -(1)- No connectivity (TCP connection can not be established)
//   -(2)- Partial connectivity (TCP connection can be established, but no data
//         transfer)
//   -(3)- Connectivity OK (TCP connection established, is healthy)
class ConnectionHealthChecker {
 public:
  typedef std::queue<IPAddress> IPAddressQueue;

  // TODO(pprabhu): Rename kResultElongatedTimeWait to kResultTearDownFailure.
  enum Result {
    // There was some problem in the setup of ConnctionHealthChecker.
    // Could not attempt a tcp connection.
    kResultUnknown,
    // New health check request made successfully. The result of the health
    // check is returned asynchronously.
    kResultInProgress,
    // Failed to create TCP connection. Condition -(1)-.
    kResultConnectionFailure,
    // Failed to destroy TCP connection. Condition -(2)-.
    kResultElongatedTimeWait,
    // Failed to send data on TCP connection. Condition -(2)-.
    kResultCongestedTxQueue,
    // Condition -(3)-.
    kResultSuccess
  };

  ConnectionHealthChecker(ConnectionRefPtr connection,
                          EventDispatcher *dispatcher,
                          const base::Callback<void(Result)> &result_callback);
  virtual ~ConnectionHealthChecker();

  // A new ConnectionHealthChecker is created with a default URL to attempt the
  // TCP connection with. Add a URL to try.
  virtual void AddRemoteURL(const std::string &url_string);

  // Name resolution can fail in conditions -(1)- and -(2)-. Add an IP address
  // to attempt the TCP connection with.
  virtual void AddRemoteIP(IPAddress ip);

  // Start a connection health check. The health check involves one or more
  // attempts at establishing and using a TCP connection. |result_callback_| is
  // called with the final result of the check. |result_callback_| will always
  // be called after a call to Start() unless Stop() is called in the meantime.
  // |result_callback_| may be called before Start() completes.
  //
  // Calling Start() while a health check is in progress is a no-op.
  virtual void Start();

  // Stop the current health check. No callback is called as a side effect of
  // this function.
  //
  // Calling Stop() on a Stop()ed health check is a no-op.
  virtual void Stop();

  static const char *ResultToString(Result result);

  // Accessors.
  const IPAddressQueue &remote_ips() { return remote_ips_; }
  void set_run_data_test(bool val) { run_data_test_ = val; }
  virtual bool health_check_in_progress() const {
      return health_check_in_progress_;
  }

 private:
  friend class ConnectionHealthCheckerTest;
  FRIEND_TEST(ConnectionHealthCheckerTest, GetSocketInfo);
  FRIEND_TEST(ConnectionHealthCheckerTest, SendData);
  FRIEND_TEST(ConnectionHealthCheckerTest, ShutDown);

  // Time to wait for DNS server.
  static const int kDNSTimeoutSeconds;
  // Number of connection attempts before failure per health check request.
  static const int kMaxConnectionAttempts;
  static const uint16 kRemotePort;

  // Start a new AsyncConnection with callback set to OnConnectionComplete().
  void SetupTcpConnection();

  // Callback for AsyncConnection.
  // Observe the setup connection to test health state
  void OnConnectionComplete(bool success, int sock_fd);

  // Callback for DnsClient
  void GetDNSResult(const Error &error, const IPAddress &ip);

  void TryNextIP();
  Result SendData(int sock_fd);
  Result ShutDown(int sock_fd);
  bool GetSocketInfo(int sock_fd, SocketInfo *sock_info);

  ConnectionRefPtr connection_;
  EventDispatcher *dispatcher_;
  base::Callback<void(Result)> result_callback_;

  IPAddressQueue remote_ips_;
  scoped_ptr<SocketInfoReader> socket_info_reader_;
  scoped_ptr<Sockets> socket_;
  base::WeakPtrFactory<ConnectionHealthChecker> weak_ptr_factory_;
  const base::Callback<void(bool, int)> connection_complete_callback_;
  const base::Callback<void(const Error&, const IPAddress&)>
      dns_client_callback_;
  scoped_ptr<AsyncConnection> tcp_connection_;
  scoped_ptr<DNSClient> dns_client_;
  // If true, HealthChecker attempts to send a small amount of data over
  // the network during the test. Otherwise, the inference is based on
  // the connection open/close behaviour.
  // Default: true
  bool run_data_test_;
  bool health_check_in_progress_;
  short num_connection_attempts_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionHealthChecker);
};

}  // namespace shill

#endif  // SHILL_CONNECTION_HEALTH_CHECKER_H_
