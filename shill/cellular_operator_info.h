// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_OPERATOR_INFO_H_
#define SHILL_CELLULAR_OPERATOR_INFO_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/scoped_vector.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/cellular_service.h"
#include "shill/file_reader.h"
#include "shill/key_file_store.h"

namespace shill {

class CellularOperatorInfo {
 public:
  CellularOperatorInfo();
  virtual ~CellularOperatorInfo();

  struct LocalizedName {
    std::string name;
    std::string language;
  };

  class MobileAPN {
   public:
    MobileAPN();
    ~MobileAPN();

    const std::string &apn() const { return apn_; }
    const std::vector<LocalizedName> &name_list() const { return name_list_; }
    const std::string &username() const { return username_; }
    const std::string &password() const { return password_; }

   private:
    friend class CellularOperatorInfo;

    std::string apn_;
    std::vector<LocalizedName> name_list_;
    std::string username_;
    std::string password_;

    DISALLOW_COPY_AND_ASSIGN(MobileAPN);
  };

  class CellularOperator {
   public:
    CellularOperator();
    virtual ~CellularOperator();

    const std::string &country() const { return country_; }
    const std::vector<std::string> &mccmnc_list() const {
        return mccmnc_list_;
    }
    const std::vector<std::string> &sid_list() const { return sid_list_; }
    const std::vector<LocalizedName> &name_list() const { return name_list_; }
    const ScopedVector<MobileAPN> &apn_list() const { return apn_list_; }
    const ScopedVector<CellularService::OLP> &olp_list() const {
        return olp_list_;
    }
    bool is_primary() const { return is_primary_; }
    bool requires_roaming() const { return requires_roaming_; }

   private:
    friend class CellularOperatorInfo;
    friend class CellularOperatorInfoTest;
    FRIEND_TEST(CellularOperatorInfoTest, HandleMCCMNC);
    FRIEND_TEST(CellularOperatorInfoTest, HandleSID);

    std::string country_;
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

  // Loads the operator info from |info_file_path|. Returns true on success.
  virtual bool Load(const FilePath &info_file_path);

  // Gets the cellular operator info of the operator with MCCMNC |mccmnc|.
  // If found, returns a pointer to the matching operator.
  virtual const CellularOperator *
      GetCellularOperatorByMCCMNC(const std::string &mccmnc);

  // Gets the cellular operator info of the operator with SID |sid|.
  // If found, returns a pointer to the matching operator.
  virtual const CellularOperator *
      GetCellularOperatorBySID(const std::string &sid);

  // Gets the cellular operator info of the operators that match the name
  // |name|, such that each element contains information about the operator
  // in different countries. The given name must be the first enumerated name
  // for the operator in the operator database.
  // If found, returns a pointer to a vector containing the matching operators.
  virtual const std::vector<const CellularOperator *> *
      GetCellularOperatorsByName(const std::string &name);

  // Gets the online payment portal info of the operator with MCCMNC |mccmnc|.
  // If found, returns a pointer to the matching OLP.
  virtual const CellularService::OLP *GetOLPByMCCMNC(const std::string &mccmnc);

  // Gets the online payment portal info of the operator with SID |sid|.
  // If found, returns a pointer to the matching OLP.
  virtual const CellularService::OLP *GetOLPBySID(const std::string &sid);

  // Returns a list of all operators.
  virtual const ScopedVector<CellularOperator> &operators() const;

 private:
  friend class CellularOperatorInfoTest;
  FRIEND_TEST(CellularOperatorInfoTest, HandleAPN);
  FRIEND_TEST(CellularOperatorInfoTest, HandleAPNName);
  FRIEND_TEST(CellularOperatorInfoTest, HandleMCCMNC);
  FRIEND_TEST(CellularOperatorInfoTest, HandleName);
  FRIEND_TEST(CellularOperatorInfoTest, HandleOLP);
  FRIEND_TEST(CellularOperatorInfoTest, HandleProvider);
  FRIEND_TEST(CellularOperatorInfoTest, HandleSID);
  FRIEND_TEST(CellularOperatorInfoTest, ParseKeyValue);
  FRIEND_TEST(CellularOperatorInfoTest, ParseNameLine);
  FRIEND_TEST(CellularOperatorInfoTest, ParseSuccess);

  struct ParserState {
    ParserState() : provider(NULL),
                    apn(NULL),
                    parsing_apn(false) {}

    std::string country;
    CellularOperator *provider;
    MobileAPN *apn;

    bool parsing_apn;
  };

  bool HandleProvider(ParserState *state,
                      const std::string &value);
  bool HandleMCCMNC(ParserState *state,
                    const std::string &value);
  bool HandleNameKey(ParserState *state,
                     const std::string &value);
  bool HandleName(ParserState *state,
                  const std::string &value);
  bool HandleAPN(ParserState *state,
                 const std::string &value);
  bool HandleAPNName(ParserState *state,
                     const std::string &value);
  bool HandleSID(ParserState *state,
                 const std::string &value);
  bool HandleOLP(ParserState *state,
                 const std::string &value);
  bool HandleCountry(ParserState *state,
                     const std::string &value);
  bool ParseNameLine(const std::string &value,
                     LocalizedName *name);

  void ClearOperators();

  // Functions that return specially formatted strings for logging
  // MCCMNC and SID to help with scrubbing.
  static std::string FormattedMCCMNC(const std::string &mccmnc);
  static std::string FormattedSID(const std::string &sid);

  // Splits |line| into two strings, separated with the |key_value_delimiter|.
  // Returns |false| if line does not contain |key_value_delimiter|, otherwise
  // returns the first substring in |key| and the second substring in |value|,
  // and returns true.
  static bool ParseKeyValue(const std::string &line,
                            char key_value_delimiter,
                            std::string *key,
                            std::string *value);

  ScopedVector<CellularOperator> operators_;
  std::map<std::string, CellularOperator *> mccmnc_to_operator_;
  std::map<std::string, CellularOperator *> sid_to_operator_;
  std::map<std::string,
           std::vector<const CellularOperator *> > name_to_operators_;

  DISALLOW_COPY_AND_ASSIGN(CellularOperatorInfo);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_OPERATOR_INFO_H_
