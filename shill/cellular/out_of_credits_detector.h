//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef SHILL_CELLULAR_OUT_OF_CREDITS_DETECTOR_H_
#define SHILL_CELLULAR_OUT_OF_CREDITS_DETECTOR_H_

#include <memory>

#include <base/macros.h>

#include "shill/cellular/subscription_state.h"
#include "shill/service.h"

namespace shill {

class CellularService;

// Base class for the various out-of-credits detection mechanism.
class OutOfCreditsDetector {
 public:
  explicit OutOfCreditsDetector(CellularService* service);
  virtual ~OutOfCreditsDetector();

  // Various types of out-of-credits detections.
  enum OOCType {
    // No out-of-credits detection is employed.
    OOCTypeNone,
    // Use ModemManager SubscriptionState property to determine OOC condition.
    OOCTypeSubscriptionState,
  };

  // Creates a specific out-of-credits detector.
  // For OOCTypeNone, this methods returns NoOutOfCreditsDetector.
  // For OOCTypeSubscriptionState, this method returns
  // SubscriptionStateOutOfCreditsDetector.
  static std::unique_ptr<OutOfCreditsDetector> CreateDetector(
      OOCType detector_type, CellularService* service);

  // Resets the detector state.
  virtual void ResetDetector() = 0;
  // Returns |true| if this object is busy detecting out-of-credits.
  virtual bool IsDetecting() const = 0;
  // Notifies this object of a service state change.
  virtual void NotifyServiceStateChanged(Service::ConnectState old_state,
                                         Service::ConnectState new_state) = 0;
  // Notifies this object when the subscription state has changed.
  virtual void NotifySubscriptionStateChanged(
      SubscriptionState subscription_state) = 0;

  virtual bool out_of_credits() const { return out_of_credits_; }

 protected:
  // Sets the out-of-credits state for this object and also tells the service
  // object to signal the property change.
  void ReportOutOfCredits(bool state);

  // Property accessors reserved for subclasses.
  CellularService* service() const { return service_; }

 private:
  CellularService* service_;
  // Flag indicating if the account is out-of-credits.
  bool out_of_credits_;

  DISALLOW_COPY_AND_ASSIGN(OutOfCreditsDetector);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_OUT_OF_CREDITS_DETECTOR_H_
