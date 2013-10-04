// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_ACTIVE_PASSIVE_OUT_OF_CREDITS_DETECTOR_H_
#define SHILL_ACTIVE_PASSIVE_OUT_OF_CREDITS_DETECTOR_H_

#include "shill/connection_health_checker.h"
#include "shill/out_of_credits_detector.h"

namespace shill {

// Detects out-of-credits condition by monitoring for the following scenarios:
//   - Passively watch for network congestion and launch active probes to
//     determine if the network has stopped routing traffic.
//   - Watch for connect/disconnect loop.
class ActivePassiveOutOfCreditsDetector : public OutOfCreditsDetector {
 public:
  ActivePassiveOutOfCreditsDetector(EventDispatcher *dispatcher,
                                    Manager *manager,
                                    Metrics *metrics,
                                    CellularService *service);
  virtual ~ActivePassiveOutOfCreditsDetector();

  virtual void ResetDetector() override;
  virtual bool IsDetecting() const override;
  virtual void NotifyServiceStateChanged(
      Service::ConnectState old_state,
      Service::ConnectState new_state) override;
  virtual void NotifySubscriptionStateChanged(
      uint32 subscription_state) override {}

  const TrafficMonitor *traffic_monitor() const {
    return traffic_monitor_.get();
  }

 private:
  friend class ActivePassiveOutOfCreditsDetectorTest;
  FRIEND_TEST(ActivePassiveOutOfCreditsDetectorTest,
      ConnectDisconnectLoopDetectionIntermittentNetwork);
  FRIEND_TEST(ActivePassiveOutOfCreditsDetectorTest,
      ConnectDisconnectLoopDetectionNotSkippedAfterSlowResume);
  FRIEND_TEST(ActivePassiveOutOfCreditsDetectorTest,
      OnConnectionHealthCheckerResult);
  FRIEND_TEST(ActivePassiveOutOfCreditsDetectorTest, OnNoNetworkRouting);
  FRIEND_TEST(ActivePassiveOutOfCreditsDetectorTest, StopTrafficMonitor);

  static const int64 kOutOfCreditsConnectionDropSeconds;
  static const int kOutOfCreditsMaxConnectAttempts;
  static const int64 kOutOfCreditsResumeIgnoreSeconds;

  // Initiates traffic monitoring.
  bool StartTrafficMonitor();

  // Stops traffic monitoring.
  void StopTrafficMonitor();

  // Responds to a TrafficMonitor no-network-routing failure.
  void OnNoNetworkRouting();

  // Initializes and configures the connection health checker.
  void SetupConnectionHealthChecker();

  // Checks the network connectivity status by creating a TCP connection, and
  // optionally sending a small amout of data.
  void RequestConnectionHealthCheck();

  // Responds to the result from connection health checker in a device specific
  // manner.
  void OnConnectionHealthCheckerResult(ConnectionHealthChecker::Result result);

  // Performs out-of-credits detection by checking to see if we're stuck in a
  // connect/disconnect loop.
  void DetectConnectDisconnectLoop(Service::ConnectState curr_state,
                                   Service::ConnectState new_state);
  // Reconnects to the cellular service in the context of out-of-credits
  // detection.
  void OutOfCreditsReconnect();

  // Ownership of |traffic_monitor| is taken.
  void set_traffic_monitor(TrafficMonitor *traffic_monitor);

  // Ownership of |healther_checker| is taken.
  void set_connection_health_checker(ConnectionHealthChecker *health_checker);

  base::WeakPtrFactory<ActivePassiveOutOfCreditsDetector> weak_ptr_factory_;

  // Passively monitors network traffic for network failures.
  scoped_ptr<TrafficMonitor> traffic_monitor_;
  // Determine network health through active probes.
  scoped_ptr<ConnectionHealthChecker> health_checker_;

  // The following members are used by the connect/disconnect loop detection.
  // Time when the last connect request started.
  base::Time connect_start_time_;
  // Number of connect attempts.
  int num_connect_attempts_;
  // Flag indicating whether out-of-credits detection is in progress.
  bool out_of_credits_detection_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(ActivePassiveOutOfCreditsDetector);
};

}  // namespace shill

#endif  // SHILL_ACTIVE_PASSIVE_OUT_OF_CREDITS_DETECTOR_H_
