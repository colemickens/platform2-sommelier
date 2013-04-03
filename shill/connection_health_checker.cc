// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/connection_health_checker.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include <base/bind.h>

#include "shill/async_connection.h"
#include "shill/connection.h"
#include "shill/dns_client.h"
#include "shill/error.h"
#include "shill/http_url.h"
#include "shill/ip_address.h"
#include "shill/logging.h"
#include "shill/sockets.h"
#include "shill/socket_info.h"
#include "shill/socket_info_reader.h"

using base::Bind;
using std::string;
using std::vector;

namespace shill {

const int ConnectionHealthChecker::kDNSTimeoutSeconds = 5;
const int ConnectionHealthChecker::kMaxConnectionAttempts = 3;
const uint16 ConnectionHealthChecker::kRemotePort = 80;

ConnectionHealthChecker::ConnectionHealthChecker(
    ConnectionRefPtr connection,
    EventDispatcher *dispatcher,
    const base::Callback<void(Result)> &result_callback)
    : connection_(connection),
      dispatcher_(dispatcher),
      result_callback_(result_callback),
      socket_info_reader_(new SocketInfoReader()),
      socket_(new Sockets()),
      weak_ptr_factory_(this),
      connection_complete_callback_(
          Bind(&ConnectionHealthChecker::OnConnectionComplete,
               weak_ptr_factory_.GetWeakPtr())),
      dns_client_callback_(Bind(&ConnectionHealthChecker::GetDNSResult,
                                weak_ptr_factory_.GetWeakPtr())),
      tcp_connection_(new AsyncConnection(connection_->interface_name(),
                                          dispatcher_,
                                          socket_.get(),
                                          connection_complete_callback_)),
      dns_client_(new DNSClient(IPAddress::kFamilyIPv4,
                                connection->interface_name(),
                                connection->dns_servers(),
                                kDNSTimeoutSeconds * 1000,
                                dispatcher,
                                dns_client_callback_)),
      run_data_test_(true),
      health_check_in_progress_(false),
      num_connection_attempts_(0) {}

ConnectionHealthChecker::~ConnectionHealthChecker() {
  Stop();
}

void ConnectionHealthChecker::AddRemoteIP(IPAddress ip) {
  remote_ips_.push(ip);
}

void ConnectionHealthChecker::AddRemoteURL(const string &url_string) {
  HTTPURL url;
  if (!url.ParseFromString(url_string)) {
    SLOG(Connection, 2) << __func__ << ": Malformed url: " << url_string << ".";
    return;
  }
  if (url.port() != kRemotePort) {
    SLOG(Connection, 2) << __func__ << ": Remote connections only supported "
                        << " to port 80, requested " << url.port() << ".";
    return;
  }
  Error error;
  if (!dns_client_->Start(url.host(), &error)) {
    SLOG(Connection, 2) << __func__ << ": Failed to start DNS client: "
                        << error.message();
  }
}

void ConnectionHealthChecker::Start() {
  if (health_check_in_progress_) {
    SLOG(Connection, 2) << __func__ << ": Health Check already in progress.";
    return;
  }
  if (!connection_.get()) {
    SLOG(Connection, 2) << __func__ << ": Connection not ready yet.";
    result_callback_.Run(kResultUnknown);
    return;
  }

  health_check_in_progress_ = true;
  num_connection_attempts_ = 0;

  // Initiate the first attempt.
  if (remote_ips_.empty()) {
    // Nothing to try.
    Stop();
    SLOG(Connection, 2) << __func__ << ": Not enough IPs.";
    result_callback_.Run(kResultUnknown);
    return;
  }
  SetupTcpConnection();
}

void ConnectionHealthChecker::Stop() {
  if (tcp_connection_ != NULL)
    tcp_connection_->Stop();
  health_check_in_progress_ = false;
}

void ConnectionHealthChecker::SetupTcpConnection() {
  IPAddress ip = remote_ips_.front();
  if (tcp_connection_->Start(ip, kRemotePort)) {
    // TCP connection successful, no need to try more.
    return;
  }

  SLOG(Connection, 2) << __func__ << ": Connection attempt failed.";
  TryNextIP();
}

void ConnectionHealthChecker::OnConnectionComplete(bool success, int sock_fd) {
  if (!success) {
    SLOG(Connection, 2) << __func__
                        << ": AsyncConnection connection attempt failed.";
    TryNextIP();  // Make sure TryNextIP() is the last statement.
    return;
  }
  // Transferred owndership of valid sock_fd.

  // Check if the established connection is healthy.
  Result result = run_data_test_ ? SendData(sock_fd) : ShutDown(sock_fd);

  // The health check routine(s) may further indicate a problem requiring a
  // reattempt.
  if (result == kResultConnectionFailure || result == kResultUnknown) {
    socket_->Close(sock_fd);
    TryNextIP();  // Make sure TryNextIP() is the last statement.
  } else {
    socket_->Close(sock_fd);
    Stop();
    result_callback_.Run(result);  // Make sure this is the last statement.
  }
}

void ConnectionHealthChecker::GetDNSResult(const Error &error,
                                           const IPAddress& ip) {
  if (!error.IsSuccess()) {
    SLOG(Connection, 2) << __func__ << "DNSClient returned failure: "
                        << error.message();
    return;
  }
  remote_ips_.push(ip);
}

void ConnectionHealthChecker::TryNextIP() {
  ++num_connection_attempts_;
  // Check if enough attempts have been made already.
  if (num_connection_attempts_ >= kMaxConnectionAttempts ||
      static_cast<IPAddressQueue::size_type>(num_connection_attempts_)
           >= remote_ips_.size()) {
    LOG(INFO) << __func__
              << ": multiple failed attempts to established a TCP connection.";
    // Give up. Clean up and notify client.
    Stop();
    result_callback_.Run(kResultConnectionFailure);
    return;
  }
  IPAddress recycle_addr = remote_ips_.front();
  remote_ips_.pop();
  remote_ips_.push(recycle_addr);

  SetupTcpConnection();
}

// Send data on the connection and observe the TxCount.
ConnectionHealthChecker::Result ConnectionHealthChecker::SendData(int sock_fd) {
  SocketInfo sock_info;
  uint64 old_transmit_queue_value;
  if (!GetSocketInfo(sock_fd, &sock_info) ||
      sock_info.connection_state() !=
          SocketInfo::kConnectionStateEstablished) {
    SLOG(Connection, 2) << __func__
                        << ": Connection originally not in established state..";
    // Count this as a failed connection attempt.
    return kResultUnknown;
  }
  old_transmit_queue_value = sock_info.transmit_queue_value();

  char buf;
  if (socket_->Send(sock_fd, &buf, sizeof(buf), 0) == -1) {
    SLOG(Connection, 2) << __func__ << ": " << socket_->ErrorString();
    // Count this as a failed connection attempt.
    return kResultConnectionFailure;
  }

  // Wait to give enough time for the TxCount to be updated.
  // TODO(pprabhu) Check that this is reliable wrt timing effects.
  if (!GetSocketInfo(sock_fd, &sock_info) ||
      sock_info.connection_state() !=
          SocketInfo::kConnectionStateEstablished) {
    SLOG(Connection, 2) << __func__
                        << ": Connection not in established state after send.";
    // Count this as a failed connection attempt.
    return kResultUnknown;
  }

  if (sock_info.transmit_queue_value() > old_transmit_queue_value) {
    return kResultCongestedTxQueue;
  }

  return kResultSuccess;
}

// Attempt to shutdown the connection and check if the connection is stuck in
// the TIME_WAIT tcp state.
ConnectionHealthChecker::Result ConnectionHealthChecker::ShutDown(int sock_fd) {
  if (socket_->ShutDown(sock_fd, SHUT_RDWR) == -1) {
    SLOG(Connection, 2) << __func__
                        << ": Failed to cleanly shut down the connection.";
    // Count this as a failed connection attempt.
    return kResultUnknown;
  }
  // Wait to give enough time for a normal TCP shutdown?
  // TODO(pprabhu) Check that this is reliable wrt timing effects.

  SocketInfo sock_info;
  if (!GetSocketInfo(sock_fd, &sock_info)) {
    // The TCP socket for the connection has been cleaned.
    // This means ShutDown was successful.
    return kResultSuccess;
  }
  if (sock_info.connection_state() == SocketInfo::kConnectionStateFinWait1 ||
      sock_info.connection_state() == SocketInfo::kConnectionStateFinWait2 ||
      sock_info.connection_state() == SocketInfo::kConnectionStateTimeWait)
    return kResultElongatedTimeWait;

  return kResultUnknown;
}

//TODO(pprabhu): Scrub IP address logging.
bool ConnectionHealthChecker::GetSocketInfo(int sock_fd,
                                            SocketInfo *sock_info) {
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  if (socket_->GetSockName(sock_fd,
                           reinterpret_cast<struct sockaddr *>(&addr),
                           &addrlen) != 0) {
    SLOG(Connection, 2) << __func__
                        << ": Failed to get address of created socket.";
    return false;
  }
  if (addr.ss_family != AF_INET) {
    SLOG(Connection, 2) << __func__ << ": IPv6 socket address found.";
    return false;
  }

  CHECK_EQ(sizeof(struct sockaddr_in), addrlen);
  struct sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(&addr);
  uint16 local_port = ntohs(addr_in->sin_port);
  char ipstr[INET_ADDRSTRLEN];
  const char *res = inet_ntop(AF_INET, &addr_in->sin_addr,
                              ipstr, sizeof(ipstr));
  if (res == NULL) {
    SLOG(Connection, 2) << __func__
                        << ": Could not convert IP address to string.";
    return false;
  }

  IPAddress local_ip_address(IPAddress::kFamilyIPv4);
  CHECK(local_ip_address.SetAddressFromString(ipstr));
  SLOG(Connection, 3) << "Local IP = " << local_ip_address.ToString()
                      << ":" << local_port;

  vector<SocketInfo> info_list;
  if (!socket_info_reader_->LoadTcpSocketInfo(&info_list)) {
    SLOG(Connection, 2) << __func__ << ": Failed to load TCP socket info.";
    return false;
  }

  for (vector<SocketInfo>::const_iterator info_list_it = info_list.begin();
       info_list_it != info_list.end();
       ++info_list_it) {
    const SocketInfo &cur_sock_info = *info_list_it;

    SLOG(Connection, 3)
        << "Testing against IP = "
        << cur_sock_info.local_ip_address().ToString()
        << ":" << cur_sock_info.local_port()
        << " (addresses equal:"
        << cur_sock_info.local_ip_address().Equals(local_ip_address)
        << ", ports equal:" << (cur_sock_info.local_port() == local_port)
        << ")";

    if (cur_sock_info.local_ip_address().Equals(local_ip_address) &&
        cur_sock_info.local_port() == local_port) {
      // Copy SocketInfo.
      *sock_info = cur_sock_info;
      return true;
    }
  }

  SLOG(Connection, 2) << __func__ << ": No matching TCP socket info.";
  return false;
}

}  // namespace shill
