// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_OUT_OF_CREDITS_DETECTOR_H_
#define SHILL_CELLULAR_OUT_OF_CREDITS_DETECTOR_H_

#include <base/macros.h>

#include <string>

#include "shill/service.h"

namespace shill {

class CellularService;
class EventDispatcher;
class Manager;
class Metrics;
class TrafficMonitor;

// Base class for the various out-of-credits detection mechanism.
class OutOfCreditsDetector {
 public:
  OutOfCreditsDetector(EventDispatcher* dispatcher,
                       Manager* manager,
                       Metrics* metrics,
                       CellularService* service);
  virtual ~OutOfCreditsDetector();

  // Various types of out-of-credits detections.
  enum OOCType {
    // No out-of-credits detection is employed.
    OOCTypeNone = 0,
    // Passively monitors the traffic for TX congestion and DNS failures, then
    // actively probe the network for TX congestion to determine if the
    // network has entered an OOC condition.
    OOCTypeActivePassive = 1,
    // Use ModemManager SubscriptionState property to determine OOC condition.
    OOCTypeSubscriptionState = 2
  };

  // Creates a specific out-of-credits detector.
  // For OOCTypeNone, this methods returns NoOutOfCreditsDetector. For
  // OOCTypeActivePassive, this method returns
  // ActivePassiveOutOfCreditsDetector. For OOCTypeSubscriptionState,
  // this method returns SubscriptionStateOutOfCreditsDetector.
  static OutOfCreditsDetector* CreateDetector(OOCType detector_type,
                                              EventDispatcher* dispatcher,
                                              Manager* manager,
                                              Metrics* metrics,
                                              CellularService* service);

  // Resets the detector state.
  virtual void ResetDetector() = 0;
  // Returns |true| if this object is busy detecting out-of-credits.
  virtual bool IsDetecting() const = 0;
  // Notifies this object of a service state change.
  virtual void NotifyServiceStateChanged(Service::ConnectState old_state,
                                         Service::ConnectState new_state) = 0;
  // Notifies this object when the subscription state has changed.
  virtual void NotifySubscriptionStateChanged(uint32_t subscription_state) = 0;

  virtual bool out_of_credits() const { return out_of_credits_; }

 protected:
  FRIEND_TEST(ActivePassiveOutOfCreditsDetectorTest,
      ConnectDisconnectLoopDetectionSkippedAlreadyOutOfCredits);

  // Sets the out-of-credits state for this object and also tells the service
  // object to signal the property change.
  void ReportOutOfCredits(bool state);

  // Property accessors reserved for subclasses.
  EventDispatcher* dispatcher() const { return dispatcher_; }
  Manager* manager() const { return manager_; }
  Metrics* metrics() const { return metrics_; }
  CellularService* service() const { return service_; }

 private:
  EventDispatcher* dispatcher_;
  Manager* manager_;
  Metrics* metrics_;
  CellularService* service_;
  // Flag indicating if the account is out-of-credits.
  bool out_of_credits_;

  DISALLOW_COPY_AND_ASSIGN(OutOfCreditsDetector);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_OUT_OF_CREDITS_DETECTOR_H_
