// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/link_monitor.h"

#include <vector>

#include <base/bind.h>
#include <base/stringprintf.h>
#include <base/string_util.h>

#include "shill/arp_client.h"
#include "shill/arp_packet.h"
#include "shill/byte_string.h"
#include "shill/connection.h"
#include "shill/device_info.h"
#include "shill/event_dispatcher.h"
#include "shill/ip_address.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/shill_time.h"

using base::Bind;
using base::Unretained;
using std::string;

namespace shill {

const unsigned int LinkMonitor::kTestPeriodMilliseconds = 5000;
const unsigned int LinkMonitor::kFailureThreshold = 5;
const unsigned int LinkMonitor::kMaxResponseSampleFilterDepth = 5;

LinkMonitor::LinkMonitor(const ConnectionRefPtr &connection,
                         EventDispatcher *dispatcher,
                         Metrics *metrics,
                         DeviceInfo *device_info,
                         const FailureCallback &failure_callback)
    : connection_(connection),
      dispatcher_(dispatcher),
      metrics_(metrics),
      device_info_(device_info),
      failure_callback_(failure_callback),
      broadcast_failure_count_(0),
      unicast_failure_count_(0),
      is_unicast_(false),
      response_sample_count_(0),
      response_sample_bucket_(0),
      time_(Time::GetInstance()) {}

LinkMonitor::~LinkMonitor() {
  Stop();
}

bool LinkMonitor::Start() {
  Stop();

  if (!device_info_->GetMACAddress(
    connection_->interface_index(), &local_mac_address_)) {
    LOG(ERROR) << "Could not get local MAC address.";
    metrics_->NotifyLinkMonitorFailure(
        connection_->technology(),
        Metrics::kLinkMonitorMacAddressNotFound,
        0, 0, 0);
    Stop();
    return false;
  }
  gateway_mac_address_ = ByteString(local_mac_address_.GetLength());
  send_request_callback_.Reset(
      Bind(&LinkMonitor::SendRequestTask, Unretained(this)));
  time_->GetTimeMonotonic(&started_monitoring_at_);
  return SendRequest();
}

void LinkMonitor::Stop() {
  SLOG(Link, 2) << "In " << __func__ << ".";
  local_mac_address_.Clear();
  gateway_mac_address_.Clear();
  arp_client_.reset();
  broadcast_failure_count_ = 0;
  unicast_failure_count_ = 0;
  is_unicast_ = false;
  response_sample_bucket_ = 0;
  response_sample_count_ = 0;
  receive_response_handler_.reset();
  send_request_callback_.Cancel();
  timerclear(&started_monitoring_at_);
  timerclear(&sent_request_at_);
}

unsigned int LinkMonitor::GetResponseTimeMilliseconds() const {
  return response_sample_count_ ?
      response_sample_bucket_ / response_sample_count_ : 0;
}

void LinkMonitor::AddResponseTimeSample(
    unsigned int response_time_milliseconds) {
  SLOG(Link, 2) << "In " << __func__ << " with sample "
                << response_time_milliseconds << ".";
  metrics_->NotifyLinkMonitorResponseTimeSampleAdded(
      connection_->technology(), response_time_milliseconds);
  response_sample_bucket_ += response_time_milliseconds;
  if (response_sample_count_ < kMaxResponseSampleFilterDepth) {
    ++response_sample_count_;
  } else {
    response_sample_bucket_ =
        response_sample_bucket_ * kMaxResponseSampleFilterDepth /
            (kMaxResponseSampleFilterDepth + 1);
  }
}

// static
string LinkMonitor::HardwareAddressToString(const ByteString &address) {
  std::vector<string> address_parts;
  for (size_t i = 0; i < address.GetLength(); ++i) {
    address_parts.push_back(
        base::StringPrintf("%02x", address.GetConstData()[i]));
  }
  return JoinString(address_parts, ':');
}

bool LinkMonitor::CreateClient() {
  arp_client_.reset(new ArpClient(connection_->interface_index()));

  if (!arp_client_->Start()) {
    return false;
  }
  SLOG(Link, 4) << "Created ARP client; listening on socket "
                << arp_client_->socket() << ".";
  receive_response_handler_.reset(
    dispatcher_->CreateReadyHandler(
        arp_client_->socket(),
        IOHandler::kModeInput,
        Bind(&LinkMonitor::ReceiveResponse, Unretained(this))));
  return true;
}

bool LinkMonitor::AddMissedResponse() {
  SLOG(Link, 2) << "In " << __func__ << ".";
  AddResponseTimeSample(kTestPeriodMilliseconds);

  if (is_unicast_) {
    ++unicast_failure_count_;
  } else {
    ++broadcast_failure_count_;
  }

  if (unicast_failure_count_ + broadcast_failure_count_ >= kFailureThreshold) {
    LOG(ERROR) << "Link monitor has reached the failure threshold with "
               << broadcast_failure_count_
               << " broadcast failures and "
               << unicast_failure_count_
               << " unicast failures.";
    failure_callback_.Run();

    struct timeval now, elapsed_time;
    time_->GetTimeMonotonic(&now);
    timersub(&now, &started_monitoring_at_, &elapsed_time);

    metrics_->NotifyLinkMonitorFailure(
        connection_->technology(),
        Metrics::kLinkMonitorFailureThresholdReached,
        elapsed_time.tv_sec,
        broadcast_failure_count_,
        unicast_failure_count_);

    Stop();
    return true;
  }
  is_unicast_ = !is_unicast_;
  return false;
}

bool LinkMonitor::IsGatewayFound() const {
  return !gateway_mac_address_.IsZero();
}

void LinkMonitor::ReceiveResponse(int fd) {
  SLOG(Link, 2) << "In " << __func__ << ".";
  ArpPacket packet;
  ByteString sender;
  if (!arp_client_->ReceiveReply(&packet, &sender)) {
    return;
  }

  if (!connection_->local().address().Equals(
           packet.remote_ip_address().address())) {
    SLOG(Link, 4) << "Response is not for our IP address.";
    return;
  }

  if (!local_mac_address_.Equals(packet.remote_mac_address())) {
    SLOG(Link, 4) << "Response is not for our MAC address.";
    return;
  }

  if (!connection_->gateway().address().Equals(
           packet.local_ip_address().address())) {
    SLOG(Link, 4) << "Response is not from the gateway IP address.";
    return;
  }

  struct timeval now, elapsed_time;
  time_->GetTimeMonotonic(&now);
  timersub(&now, &sent_request_at_, &elapsed_time);

  AddResponseTimeSample(elapsed_time.tv_sec * 1000 +
                        elapsed_time.tv_usec / 1000);

  receive_response_handler_.reset();
  arp_client_.reset();

  if (is_unicast_) {
    unicast_failure_count_ = 0;
  } else {
    broadcast_failure_count_ = 0;
  }

  if (!gateway_mac_address_.Equals(packet.local_mac_address())) {
    const ByteString &new_mac_address = packet.local_mac_address();
    if (!IsGatewayFound()) {
      SLOG(Link, 2) << "Found gateway at "
                    << HardwareAddressToString(new_mac_address);
    } else {
      SLOG(Link, 2) << "Gateway MAC address changed.";
    }
    gateway_mac_address_ = new_mac_address;
  }

  is_unicast_ = !is_unicast_;
}

bool LinkMonitor::SendRequest() {
  SLOG(Link, 2) << "In " << __func__ << ".";
  if (!arp_client_.get()) {
    if (!CreateClient()) {
      LOG(ERROR) << "Failed to start ARP client.";
      Stop();
      metrics_->NotifyLinkMonitorFailure(
          connection_->technology(),
          Metrics::kLinkMonitorClientStartFailure,
          0, 0, 0);
      return false;
    }
  } else if (AddMissedResponse()) {
    // If an ARP client is still listening, this means we have timed
    // out reception of the ARP reply.
    return false;
  } else {
    // We already have an ArpClient instance running.  These aren't
    // bound sockets in the conventional sense, and we cannot distinguish
    // which request (from which trial, or even from which component
    // in the local system) an ARP reply was sent in response to.
    // Therefore we keep the already open ArpClient in the case of
    // a non-fatal timeout.
  }

  ByteString destination_mac_address(gateway_mac_address_.GetLength());
  if (!IsGatewayFound()) {
    // The remote MAC addess is set by convention to be all-zeroes in the
    // ARP header if not known.  The ArpClient will translate an all-zeroes
    // remote address into a send to the broadcast (all-ones) address in
    // the Ethernet frame header.
    SLOG_IF(Link, 2, is_unicast_) << "Sending broadcast since "
                                  << "gateway MAC is unknown";
    is_unicast_ = false;
  } else if (is_unicast_) {
    destination_mac_address = gateway_mac_address_;
  }

  ArpPacket request(connection_->local(), connection_->gateway(),
                    local_mac_address_, destination_mac_address);
  if (!arp_client_->TransmitRequest(request)) {
    LOG(ERROR) << "Failed to send ARP request.  Stopping.";
    Stop();
    metrics_->NotifyLinkMonitorFailure(
        connection_->technology(), Metrics::kLinkMonitorTransmitFailure,
        0, 0, 0);
    return false;
  }

  time_->GetTimeMonotonic(&sent_request_at_);

  dispatcher_->PostDelayedTask(send_request_callback_.callback(),
                               kTestPeriodMilliseconds);
  return true;
}

void LinkMonitor::SendRequestTask() {
  SendRequest();
}

}  // namespace shill
