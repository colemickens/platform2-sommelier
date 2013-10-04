// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOBILE_OPERATOR_H_
#define SHILL_MOBILE_OPERATOR_H_

#include <string>

#include <base/observer_list.h>

#include "shill/accessor_interface.h"

namespace shill {

class ModemInfo;

// MobileOperator contains information related to the current cellular
// carrier based on data read from the Modem and provides a common place to
// access this information from.
class MobileOperator {
 public:
  enum OperatorCodeType {
    kOperatorCodeTypeSID,
    kOperatorCodeTypeMCCMNC
  };

  // Observer interface used to notify interested classes that data has been
  // updated.
  class Observer {
   public:
    virtual void OnHomeProviderInfoChanged(const MobileOperator *handler) = 0;
    virtual void OnServingOperatorInfoChanged(
        const MobileOperator *handler) = 0;
    virtual void OnApnListChanged(const MobileOperator *handler) = 0;
    virtual void OnOnlinePaymentUrlTemplateChanged(
        const MobileOperator *handler) = 0;
  };

  explicit MobileOperator(ModemInfo *modem_info);

  // Add/remove observers to subscribe to notifications.
  void AddObserver(Observer *observer);
  void RemoveObserver(Observer *observer);

  // This should be called, when operator data is received OTA. The result of
  // this operation dictates the serving operator, APN list and the online
  // payment URL. This method will do its best to fill in the operator
  // information from the databases.
  //
  // If |operator_code| is missing, and no best match is found based on
  // |operator_name|, the contents of the serving operator will be cleared. If
  // neither |operator_code| nor |operator_name| match an entry in the
  // databases, the serving operator will be updated based on these arguments.
  void OtaOperatorInfoReceived(const std::string &operator_code,
                               const std::string &operator_name,
                               OperatorCodeType type);

  // This should be called, when operator data is received from the SIM card.
  // The result of this operation dictates the home provider.
  //
  // If |operator_code| is missing, and no best match is found based on
  // |operator_name|, the contents of the home provider will be cleared. If
  // neither |operator_code| nor |operator_name| match an entry in the
  // databases, the home provider will be updated based on these arguments.
  //
  // For this method, |operator_code| is always in the MCCMNC format, as SIMs
  // don't report SIDs. If an SID is passed for |operator_code|, the method
  // will interpret it as an MCCMNC value.
  void SimOperatorInfoReceived(const std::string &operator_code,
                               const std::string &operator_name);

  // Provider information. A user's home provider is the carrier they purchased
  // their data plan from, whereas the serving operator is the current cellular
  // operator that is feeding their data. These two are usually the same,
  // except in the cases of roaming and CDMA. The format of the returned
  // dictionary is:
  //    {
  //      "name": <operator-name>,
  //      "code": <operator-code>,
  //      "country": <operator-country>
  //    }
  // The keys correspond to shill::kOperatorNameKey, shill::kOperatorCodeKey,
  // and shill::kOperatorCountryKey respectively, as defined in
  // service_constants.h. If any of the above keys is not known, there will be
  // no entry in the dictionary for it.
  const Stringmap &home_provider() const { return home_provider_; }
  const Stringmap &serving_operator() const { return serving_operator_; }

  // Known access points related to the current serving operator. Possible keys
  // are defined in service_constants.h as shill::kApn*.
  const Stringmaps &apn_list() const { return apn_list_; }

  // The online payment URL, when available, is used by Chrome to access the
  // carrier's service activation portal it is only available for select
  // carriers. Possible keys are:
  //    {
  //      "url": <the url>,
  //      "method": <HTTP method>,
  //      "postdata": <argument template for the url>
  //    }
  // The argument template is used to construct the POST arguments for the URL
  // using information such as ICCID, IMEI, etc.
  // TODO(armansito): Define constants for the above keys in
  // service_constants.h (Also see TODO in cellular_service.cc:35).
  const Stringmap &online_payment_url_template() const {
    return online_payment_url_template_;
  }

  bool home_provider_requires_roaming() const {
    return home_provider_requires_roaming_;
  }

 private:
  // These methods notify observers of events.
  void NotifyHomeProviderInfoChanged();
  void NotifyServingOperatorInfoChanged();
  void NotifyApnListChanged();
  void NotifyOnlinePaymentUrlTemplateChanged();

  // It is OK to use a raw pointer here, as:
  //   1. ModemInfo is not owned by MobileOperator.
  //   2. ModemInfo has a longer life-time than MobileOperator.
  ModemInfo *modem_info_;

  Stringmap home_provider_;
  Stringmap serving_operator_;
  Stringmaps apn_list_;
  Stringmap online_payment_url_template_;
  bool home_provider_requires_roaming_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MobileOperator);
};

}  // namespace shill

#endif  //  SHILL_MOBILE_OPERATOR_H_
