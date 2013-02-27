// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines shill::CellularOperatorInfo, which is meant to replace the
// mobile-broadband-provider-info library by providing the same mechanisms in
// C++. It also extends the information provided by
// mobile-broadband-provider-info.

#ifndef SHILL_CELLULAR_OPERATOR_INFO_H_
#define SHILL_CELLULAR_OPERATOR_INFO_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular_service.h"
#include "shill/file_reader.h"

namespace shill {

class CellularOperatorInfoImpl;

// CellularOperatorInfo, which is a utility class for parsing
// cellular carrier specific infomation from a specially formatted file that
// encodes carrier related data in a key-value format.
class CellularOperatorInfo {
 public:
  // Encapsulates a name and the language that name has been localized to.
  // The name can be a carrier name, or the name that a cellular carrier
  // prefers to show for a certain access point.
  struct LocalizedName {
    // The name as it appears in the corresponding language.
    std::string name;

    // The language of this localized name. The format of a language is a two
    // letter language code, e.g. 'en' for English.
    // It is legal for an instance of LocalizedName to have an empty |language|
    // field, as sometimes the underlying database does not contain that
    // information.
    std::string language;
  };

  // Encapsulates information on a mobile access point name. This information
  // is usually necessary for 3GPP networks to be able to connect to a mobile
  // network. So far, CDMA networks don't use this information.
  struct MobileAPN {
    // The access point url, which is fed to the modemmanager while connecting.
    std::string apn;

    // A list of localized names for this access point. Usually there is only
    // one for each country that the associated cellular carrier operates in.
    std::vector<LocalizedName> name_list;

    // The username and password fields that are required by the modemmanager.
    // Either of these values can be empty if none is present. If a MobileAPN
    // instance that is obtained from this parser contains a non-empty value
    // for username/password, this usually means that the carrier requires
    // a certain default pair.
    std::string username;
    std::string password;
  };

  // This class contains all the necessary information for shill to register
  // with and establish a connection to a mobile network.
  class CellularOperator {
   public:
    CellularOperator();

    // For this instance, returns the primary country code that this operator
    // serves. The underlying database sometimes contains multiple entries for
    // the same carrier for different countries.
    const std::string &country() const { return country_; }

    // The unique identifier of this carrier. This is primarily used to
    // identify the user profile in store for each carrier. This identifier is
    // access technology agnostic and should be the same across 3GPP and CDMA.
    const std::string &identifier() const { return identifier_; }

    // MCCMNC or (MCC/MNC tuple) is the combination of a "Mobile Country Code"
    // and "Mobile Network Code" and is used to uniquely identify a carrier.
    // ModemManager currently return MCCMNC as the primary operator code for
    // 3GPP networks. A carrier can be associated with multiple MCCMNC values
    // based on location and technology (e.g. 3G, LTE).
    const std::vector<std::string> &mccmnc_list() const {
        return mccmnc_list_;
    }

    // The SID is the primary operator code currently used by ModemManager to
    // identify CDMA networks. There are likely many SID values associated with
    // a CDMA carrier as they vary across regions and are more fine grained
    // than countries. An important thing to keep in mind is that, since an SID
    // contains fine grained information on where a modem is physically
    // located, it should be regarded as user-sensitive information.
    const std::vector<std::string> &sid_list() const { return sid_list_; }

    // All localized names associated with this carrier entry.
    const std::vector<LocalizedName> &name_list() const { return name_list_; }

    // All access point names associated with this carrier entry.
    const ScopedVector<MobileAPN> &apn_list() const { return apn_list_; }

    // All Online Payment Portal URLs associated with this carrier entry. There
    // are usually multiple OLPs based on access technology and it is up to the
    // application to use the appropriate one.
    const ScopedVector<CellularService::OLP> &olp_list() const {
        return olp_list_;
    }

    // This flag is declared for certain carriers in the underlying database.
    // Shill currently does not use it.
    bool is_primary() const { return is_primary_; }

    // Some carriers are only available while roaming. This is mainly used by
    // Chrome.
    bool requires_roaming() const { return requires_roaming_; }

   private:
    friend class CellularOperatorInfo;
    friend class CellularOperatorInfoImpl;
    FRIEND_TEST(CellularCapabilityUniversalMainTest, UpdateStorageIdentifier);

    std::string country_;
    std::string identifier_;
    std::vector<std::string> mccmnc_list_;
    std::vector<std::string> sid_list_;
    std::vector<LocalizedName> name_list_;
    ScopedVector<MobileAPN> apn_list_;
    ScopedVector<CellularService::OLP> olp_list_;
    std::map<std::string, uint32> mccmnc_to_olp_idx_;
    std::map<std::string, uint32> sid_to_olp_idx_;
    bool is_primary_;
    bool requires_roaming_;

    DISALLOW_COPY_AND_ASSIGN(CellularOperator);
  };

  // The class Constructor and Destructor don't perform any special
  // initialization or cleanup. The primary initializer is the Load()
  // method.
  CellularOperatorInfo();
  virtual ~CellularOperatorInfo();

  // Loads the operator info from |info_file_path|. Returns true on success.
  bool Load(const base::FilePath &info_file_path);

  // Gets the cellular operator info of the operator with MCCMNC |mccmnc|.
  // If found, returns a pointer to the matching operator.
  virtual const CellularOperator *GetCellularOperatorByMCCMNC(
      const std::string &mccmnc) const;

  // Gets the cellular operator info of the operator with SID |sid|.
  // If found, returns a pointer to the matching operator.
  virtual const CellularOperator *GetCellularOperatorBySID(
      const std::string &sid) const;

  // Gets the cellular operator info of the operators that match the name
  // |name|, such that each element contains information about the operator
  // in different countries. The given name must be the first enumerated name
  // for the operator in the operator database.
  // If found, returns a pointer to a vector containing the matching operators.
  virtual const std::vector<const CellularOperator *> *GetCellularOperators(
      const std::string &name) const;

  // Gets the online payment portal info of the operator with MCCMNC |mccmnc|.
  // If found, returns a pointer to the matching OLP.
  virtual const CellularService::OLP *GetOLPByMCCMNC(
      const std::string &mccmnc) const;

  // Gets the online payment portal info of the operator with SID |sid|.
  // If found, returns a pointer to the matching OLP.
  virtual const CellularService::OLP *GetOLPBySID(const std::string &sid) const;

  // Returns a list of all operators.
  const ScopedVector<CellularOperator> &operators() const;

 private:
  scoped_ptr<CellularOperatorInfoImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(CellularOperatorInfo);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_OPERATOR_INFO_H_
