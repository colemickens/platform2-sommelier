// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_LINK_MONITOR_H_
#define SHILL_LINK_MONITOR_H_

#include <time.h>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/memory/scoped_ptr.h>

#include "shill/byte_string.h"
#include "shill/refptr_types.h"

namespace shill {

class ArpClient;
class DeviceInfo;
class EventDispatcher;
class IOHandler;
class Metrics;
class Time;

// LinkMonitor tracks the status of a connection by sending ARP
// messages to the default gateway for a connection.  It keeps
// track of response times which can be an indicator of link
// quality.  It signals to caller that the link has failed if
// too many requests go unanswered.
class LinkMonitor {
 public:
  typedef base::Closure FailureCallback;

  // When the sum of consecutive unicast and broadcast failures
  // equals this value, the failure callback is called, the counters
  // are reset, and the link monitoring quiesces.  Needed by Metrics.
  static const int kFailureThreshold;

  // The number of milliseconds between ARP requests.  Needed by Metrics.
  static const int kTestPeriodMilliseconds;

  // The default list of technologies for which link monitoring is enabled.
  static const char kDefaultLinkMonitorTechnologies[];

  LinkMonitor(const ConnectionRefPtr &connection,
              EventDispatcher *dispatcher,  // Owned by caller; can't be NULL.
              Metrics *metrics,  // Owned by caller; must not be NULL.
              DeviceInfo *device_info,
              const FailureCallback &failure_callback);
  virtual ~LinkMonitor();

  // Starts link-monitoring on the selected connection.  Returns
  // true if successful, false otherwise.
  virtual bool Start();
  virtual void Stop();

  // Return modified cumulative average of the gateway ARP response
  // time.  Returns zero if no samples are available.  For each
  // missed ARP response, the sample is assumed to be the full
  // test period.
  virtual int GetResponseTimeMilliseconds() const;

  // Returns true if the LinkMonitor was ever able to find the default
  // gateway via broadcast ARP.
  virtual bool IsGatewayFound() const;

 private:
  friend class LinkMonitorForTest;
  friend class LinkMonitorTest;

  // The number of samples to compute a "strict" average over.  When
  // more samples than this number arrive, this determines how "slow"
  // our simple low-pass filter works.
  static const int kMaxResponseSampleFilterDepth;

  // Add a response time sample to the buffer.
  void AddResponseTimeSample(int response_time_milliseconds);
  // Create an ArpClient instance so we can receive and transmit ARP
  // packets.  This method is virtual so it can be overridden in
  // unit tests.
  virtual bool CreateClient();
  // Convert a hardware address byte-string to a colon-separated string.
  static std::string HardwareAddressToString(const ByteString &address);
  // Denote a missed response.  Returns true if this loss has caused us
  // to exceed the failure threshold.
  bool AddMissedResponse();
  // This I/O callback is triggered whenever the ARP reception socket
  // has data available to be received.
  void ReceiveResponse(int fd);
  // Send the next ARP request.  Returns true if successful, false
  // otherwise.
  bool SendRequest();

  // The connection on which to perform link monitoring.
  ConnectionRefPtr connection_;
  // Dispatcher on which to create delayed tasks.
  EventDispatcher *dispatcher_;
  // Metrics instance on which to post performance results.
  Metrics *metrics_;
  // DeviceInfo instance for retrieving the MAC address of a device.
  DeviceInfo *device_info_;
  // Failure callback method to call if LinkMonitor fails.
  FailureCallback failure_callback_;
  // The MAC address of device associated with this connection.
  ByteString local_mac_address_;
  // The MAC address of the default gateway.
  ByteString gateway_mac_address_;
  // ArpClient instance used for performing link tests.
  scoped_ptr<ArpClient> arp_client_;

  // The number of consecutive times we have failed in receiving
  // responses to broadcast ARP requests.
  int broadcast_failure_count_;
  // The number of consecutive times we have failed in receiving
  // responses to unicast ARP requests.
  int unicast_failure_count_;

  // Whether this iteration of the test was a unicast request
  // to the gateway instead of broadcast.  The link monitor
  // alternates between unicast and broadcast requests so that
  // both types of network traffic is monitored.
  bool is_unicast_;

  // Number of response samples received in our rolling averge.
  int response_sample_count_;
  // The sum of response samples in our rolling average.
  int response_sample_bucket_;

  // IOCallback that fires when the socket associated with our ArpClient
  // has a packet to be received.  Calls ReceiveResponse().
  scoped_ptr<IOHandler> receive_response_handler_;
  // Callback method used for periodic transmission of ARP requests.
  // When the timer expires this will call SendRequest() through the
  // void callback function SendRequestTask().
  base::CancelableClosure send_request_callback_;

  // The time at which the link monitor started.
  struct timeval started_monitoring_at_;

  // The time at which the last ARP request was sent.
  struct timeval sent_request_at_;
  // Time instance for performing GetTimeMonotonic().
  Time *time_;

  DISALLOW_COPY_AND_ASSIGN(LinkMonitor);
};

}  // namespace shill

#endif  // SHILL_LINK_MONITOR_H_
