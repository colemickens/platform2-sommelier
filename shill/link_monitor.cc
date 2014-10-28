// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/link_monitor.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>

#include "shill/arp_client.h"
#include "shill/arp_packet.h"
#include "shill/connection.h"
#include "shill/device_info.h"
#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/net/ip_address.h"
#include "shill/net/shill_time.h"

using base::Bind;
using base::Unretained;
using std::string;

namespace shill {

const int LinkMonitor::kDefaultTestPeriodMilliseconds = 5000;
const char LinkMonitor::kDefaultLinkMonitorTechnologies[] = "wifi";
const int LinkMonitor::kFailureThreshold = 5;
const int LinkMonitor::kFastTestPeriodMilliseconds = 200;
const int LinkMonitor::kMaxResponseSampleFilterDepth = 5;
const int LinkMonitor::kUnicastReplyReliabilityThreshold = 10;

LinkMonitor::LinkMonitor(const ConnectionRefPtr &connection,
                         EventDispatcher *dispatcher,
                         Metrics *metrics,
                         DeviceInfo *device_info,
                         const FailureCallback &failure_callback,
                         const GatewayChangeCallback &gateway_change_callback)
    : connection_(connection),
      dispatcher_(dispatcher),
      metrics_(metrics),
      device_info_(device_info),
      failure_callback_(failure_callback),
      gateway_change_callback_(gateway_change_callback),
      test_period_milliseconds_(kDefaultTestPeriodMilliseconds),
      broadcast_failure_count_(0),
      unicast_failure_count_(0),
      broadcast_success_count_(0),
      unicast_success_count_(0),
      is_unicast_(false),
      gateway_supports_unicast_arp_(false),
      response_sample_count_(0),
      response_sample_bucket_(0),
      time_(Time::GetInstance()) {
}

LinkMonitor::~LinkMonitor() {
  Stop();
}

bool LinkMonitor::Start() {
  Stop();
  return StartInternal(kDefaultTestPeriodMilliseconds);
}

bool LinkMonitor::StartInternal(int probe_period_milliseconds) {
  test_period_milliseconds_ = probe_period_milliseconds;
  if (test_period_milliseconds_ > kDefaultTestPeriodMilliseconds) {
    LOG(WARNING) << "Long test period; UMA stats will be truncated.";
  }

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
  if (gateway_mac_address_.IsEmpty()) {
    gateway_mac_address_ = ByteString(local_mac_address_.GetLength());
  }
  send_request_callback_.Reset(
      Bind(base::IgnoreResult(&LinkMonitor::SendRequest), Unretained(this)));
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
  broadcast_success_count_ = 0;
  unicast_success_count_ = 0;
  is_unicast_ = false;
  gateway_supports_unicast_arp_ = false;
  response_sample_bucket_ = 0;
  response_sample_count_ = 0;
  receive_response_handler_.reset();
  send_request_callback_.Cancel();
  timerclear(&started_monitoring_at_);
  timerclear(&sent_request_at_);
}

void LinkMonitor::OnAfterResume() {
  ByteString prior_gateway_mac_address(gateway_mac_address_);
  bool gateway_supports_unicast_arp = gateway_supports_unicast_arp_;
  Stop();
  gateway_mac_address_ = prior_gateway_mac_address;
  gateway_supports_unicast_arp_ = gateway_supports_unicast_arp;
  StartInternal(kFastTestPeriodMilliseconds);
}

int LinkMonitor::GetResponseTimeMilliseconds() const {
  return response_sample_count_ ?
      response_sample_bucket_ / response_sample_count_ : 0;
}

void LinkMonitor::AddResponseTimeSample(int response_time_milliseconds) {
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

  if (!arp_client_->StartReplyListener()) {
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
  AddResponseTimeSample(test_period_milliseconds_);

  if (is_unicast_) {
    if (gateway_supports_unicast_arp_) {
      ++unicast_failure_count_;
    }
    unicast_success_count_ = 0;
  } else {
    ++broadcast_failure_count_;
    broadcast_success_count_ = 0;
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
  if (!arp_client_->ReceivePacket(&packet, &sender)) {
    return;
  }

  if (!packet.IsReply()) {
    SLOG(Link, 4) << "This is not a reply packet.  Ignoring.";
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
    ++unicast_success_count_;
    unicast_failure_count_ = 0;
    if (unicast_success_count_ >= kUnicastReplyReliabilityThreshold) {
      SLOG_IF(Link, 2, !gateway_supports_unicast_arp_)
          << "Gateway is now considered a reliable unicast responder.  "
             "Unicast failures will now count.";
      gateway_supports_unicast_arp_ = true;
    }
  } else {
    ++broadcast_success_count_;
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

    // Notify device of the new gateway mac address.
    gateway_change_callback_.Run();
  }

  is_unicast_ = !is_unicast_;
  if ((unicast_success_count_ || !gateway_supports_unicast_arp_)
      && broadcast_success_count_) {
    test_period_milliseconds_ = kDefaultTestPeriodMilliseconds;
  }
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
                               test_period_milliseconds_);
  return true;
}

}  // namespace shill
