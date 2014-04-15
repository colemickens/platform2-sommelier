// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <map>
#include <ostream>
#include <set>
#include <vector>

#include <base/basictypes.h>
#include <base/file_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/mobile_operator_info.h"
#include "shill/mobile_operator_info_impl.h"

// These files contain binary protobuf definitions used by the following tests
// inside the namespace ::mobile_operator_db
#define IN_MOBILE_OPERATOR_INFO_UNITTEST_CC
#include "mobile_operator_db/test_protos/data_test.h"
#include "mobile_operator_db/test_protos/init_test_empty_db_init.h"
#include "mobile_operator_db/test_protos/init_test_multiple_db_init_1.h"
#include "mobile_operator_db/test_protos/init_test_multiple_db_init_2.h"
#include "mobile_operator_db/test_protos/init_test_successful_init.h"
#include "mobile_operator_db/test_protos/main_test.h"
#undef IN_MOBILE_OPERATOR_INFO_UNITTEST_CC

using base::FilePath;
using shill::mobile_operator_db::MobileOperatorDB;
using std::map;
using std::ofstream;
using std::set;
using std::string;
using std::vector;
using testing::Mock;
using testing::Test;

namespace shill {

class MockMobileOperatorInfoObserver : public MobileOperatorInfo::Observer {
 public:
  MockMobileOperatorInfoObserver() {}
  virtual ~MockMobileOperatorInfoObserver() {}

  MOCK_METHOD0(OnOperatorChanged, void());
};

class MobileOperatorInfoInitTest : public Test {
 public:
  MobileOperatorInfoInitTest()
      : operator_info_(new MobileOperatorInfo(&dispatcher_)),
        operator_info_impl_(operator_info_->impl()) {}

  virtual void TearDown() override {
    for (const auto &tmp_db_path : tmp_db_paths_) {
      base::DeleteFile(tmp_db_path, false);
    }
  }

 protected:
  void AddDatabase(const unsigned char database_data[], size_t num_elems) {
    FilePath tmp_db_path;
    CHECK(base::CreateTemporaryFile(&tmp_db_path));
    tmp_db_paths_.push_back(tmp_db_path);

    ofstream tmp_db(tmp_db_path.value(), ofstream::binary);
    for (size_t i = 0; i < num_elems; ++i) {
      tmp_db << database_data[i];
    }
    tmp_db.close();
    operator_info_->AddDatabasePath(tmp_db_path);
  }

  void AssertDatabaseEmpty() {
    EXPECT_EQ(0, operator_info_impl_->database()->mno_size());
    EXPECT_EQ(0, operator_info_impl_->database()->imvno_size());
  }

  const MobileOperatorDB *GetDatabase() {
    return operator_info_impl_->database();
  }

  EventDispatcher dispatcher_;
  vector<FilePath> tmp_db_paths_;
  scoped_ptr<MobileOperatorInfo> operator_info_;
  // Owned by |operator_info_| and tied to its life cycle.
  MobileOperatorInfoImpl *operator_info_impl_;

  DISALLOW_COPY_AND_ASSIGN(MobileOperatorInfoInitTest);
};

TEST_F(MobileOperatorInfoInitTest, FailedInitNoPath) {
  // - Initialize object with no database paths set
  // - Verify that initialization fails.
  operator_info_->ClearDatabasePaths();
  EXPECT_FALSE(operator_info_->Init());
  AssertDatabaseEmpty();
}

TEST_F(MobileOperatorInfoInitTest, FailedInitBadPath) {
  // - Initialize object with non-existent path.
  // - Verify that initialization fails.
  const FilePath database_path("nonexistent.pbf");
  operator_info_->ClearDatabasePaths();
  operator_info_->AddDatabasePath(database_path);
  EXPECT_FALSE(operator_info_->Init());
  AssertDatabaseEmpty();
}

TEST_F(MobileOperatorInfoInitTest, FailedInitBadDatabase) {
  // - Initialize object with malformed database.
  // - Verify that initialization fails.
  // TODO(pprabhu): It's hard to get a malformed database in binary format.
}

TEST_F(MobileOperatorInfoInitTest, EmptyDBInit) {
  // - Initialize the object with a database file that is empty.
  // - Verify that initialization succeeds, and that the database is empty.
  operator_info_->ClearDatabasePaths();
  // Can't use arraysize on empty array.
  AddDatabase(mobile_operator_db::init_test_empty_db_init, 0);
  EXPECT_TRUE(operator_info_->Init());
  AssertDatabaseEmpty();
}

TEST_F(MobileOperatorInfoInitTest, SuccessfulInit) {
  operator_info_->ClearDatabasePaths();
  AddDatabase(mobile_operator_db::init_test_successful_init,
              arraysize(mobile_operator_db::init_test_successful_init));
  EXPECT_TRUE(operator_info_->Init());
  EXPECT_GT(GetDatabase()->mno_size(), 0);
  EXPECT_GT(GetDatabase()->imvno_size(), 0);
}

TEST_F(MobileOperatorInfoInitTest, MultipleDBInit) {
  // - Initialize the object with two database files.
  // - Verify that intialization succeeds, and both databases are loaded.
  operator_info_->ClearDatabasePaths();
  AddDatabase(mobile_operator_db::init_test_multiple_db_init_1,
              arraysize(mobile_operator_db::init_test_multiple_db_init_1));
  AddDatabase(mobile_operator_db::init_test_multiple_db_init_2,
              arraysize(mobile_operator_db::init_test_multiple_db_init_2));
  operator_info_->Init();
  EXPECT_GT(GetDatabase()->mno_size(), 0);
  EXPECT_GT(GetDatabase()->imvno_size(), 0);
}

TEST_F(MobileOperatorInfoInitTest, InitWithObserver) {
  // - Add an Observer.
  // - Initialize the object with empty database file.
  // - Verify innitialization succeeds.
  MockMobileOperatorInfoObserver dumb_observer;

  operator_info_->ClearDatabasePaths();
  // Can't use arraysize with empty array.
  AddDatabase(mobile_operator_db::init_test_empty_db_init, 0);
  operator_info_->AddObserver(&dumb_observer);
  EXPECT_TRUE(operator_info_->Init());
}

class MobileOperatorInfoMainTest : public MobileOperatorInfoInitTest {
 public:
  MobileOperatorInfoMainTest() : MobileOperatorInfoInitTest() {}

  virtual void SetUp() {
    operator_info_->ClearDatabasePaths();
    AddDatabase(mobile_operator_db::main_test,
                arraysize(mobile_operator_db::main_test));
    operator_info_->Init();
    operator_info_->AddObserver(&observer_);
  }

 protected:
  // ///////////////////////////////////////////////////////////////////////////
  // Helper functions.
  void VerifyMNOWithUUID(const string &uuid) {
    EXPECT_TRUE(operator_info_->IsMobileNetworkOperatorKnown());
    EXPECT_FALSE(operator_info_->IsMobileVirtualNetworkOperatorKnown());
    EXPECT_EQ(uuid, operator_info_->uuid());
  }

  void VerifyMVNOWithUUID(const string &uuid) {
    EXPECT_TRUE(operator_info_->IsMobileNetworkOperatorKnown());
    EXPECT_TRUE(operator_info_->IsMobileVirtualNetworkOperatorKnown());
    EXPECT_EQ(uuid, operator_info_->uuid());
  }

  void VerifyNoMatch() {
    EXPECT_FALSE(operator_info_->IsMobileNetworkOperatorKnown());
    EXPECT_FALSE(operator_info_->IsMobileVirtualNetworkOperatorKnown());
    EXPECT_EQ("", operator_info_->uuid());
  }

  void ExpectEventCount(int count) {
    EXPECT_CALL(observer_, OnOperatorChanged()).Times(count);
  }

  void VerifyEventCount() {
    dispatcher_.DispatchPendingEvents();
    Mock::VerifyAndClearExpectations(&observer_);
  }

  void ResetOperatorInfo() {
    operator_info_->Reset();
    // Eat up any events caused by |Reset|.
    dispatcher_.DispatchPendingEvents();
    VerifyNoMatch();
  }

  // ///////////////////////////////////////////////////////////////////////////
  // Data.
  MockMobileOperatorInfoObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(MobileOperatorInfoMainTest);
};

TEST_F(MobileOperatorInfoMainTest, InitialConditions) {
  // - Initialize a new object.
  // - Verify that all initial values of properties are reasonable.
  EXPECT_FALSE(operator_info_->IsMobileNetworkOperatorKnown());
  EXPECT_FALSE(operator_info_->IsMobileVirtualNetworkOperatorKnown());
  EXPECT_TRUE(operator_info_->uuid().empty());
  EXPECT_TRUE(operator_info_->operator_name().empty());
  EXPECT_TRUE(operator_info_->country().empty());
  EXPECT_TRUE(operator_info_->mccmnc().empty());
  EXPECT_TRUE(operator_info_->sid().empty());
  EXPECT_TRUE(operator_info_->nid().empty());
  EXPECT_TRUE(operator_info_->mccmnc_list().empty());
  EXPECT_TRUE(operator_info_->sid_list().empty());
  EXPECT_TRUE(operator_info_->operator_name_list().empty());
  EXPECT_TRUE(operator_info_->apn_list().empty());
  EXPECT_TRUE(operator_info_->olp_list().empty());
  EXPECT_TRUE(operator_info_->activation_code().empty());
  EXPECT_FALSE(operator_info_->requires_roaming());
}

TEST_F(MobileOperatorInfoMainTest, MNOByMCCMNC) {
  // message: Has an MNO with no MVNO.
  // match by: MCCMNC.
  // verify: Observer event, uuid.

  ExpectEventCount(0);
  operator_info_->UpdateMCCMNC("101999");  // No match.
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("101001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid101");

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("101999");
  VerifyEventCount();
  VerifyNoMatch();
}

TEST_F(MobileOperatorInfoMainTest, MNOByMCCMNCMultipleOptions) {
  // message: Has an MNO with no MCCMNC.
  // match by: One of the MCCMNCs of the multiple ones in the MNO.
  // verify: Observer event, uuid.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("102002");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid102");
}

TEST_F(MobileOperatorInfoMainTest, MNOByOperatorName) {
  // message: Has an MNO with no MVNO.
  // match by: OperatorName.
  // verify: Observer event, uuid.
  ExpectEventCount(0);
  operator_info_->UpdateOperatorName("name103999");  // No match.
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name103");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid103");

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name103999");  // No match.
  VerifyEventCount();
  VerifyNoMatch();
}

TEST_F(MobileOperatorInfoMainTest, MNOByOperatorNameWithLang) {
  // message: Has an MNO with no MVNO.
  // match by: OperatorName.
  // verify: Observer event, fields.
  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name105");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid105");
}

TEST_F(MobileOperatorInfoMainTest, MNOByOperatorNameMultipleOptions) {
  // message: Has an MNO with no MVNO.
  // match by: OperatorName, one of the multiple present in the MNO.
  // verify: Observer event, fields.
  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name104002");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid104");
}

TEST_F(MobileOperatorInfoMainTest, MNOByMCCMNCAndOperatorName) {
  // message: Has MNOs with no MVNO.
  // match by: MCCMNC finds two candidates, Name narrows down to one.
  // verify: Observer event, fields.
  // This is merely a MCCMNC update.
  ExpectEventCount(0);
  operator_info_->UpdateMCCMNC("106001");
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name106002");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid106002");

  ResetOperatorInfo();
  // Try updates in reverse order.
  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name106001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid106001");
}

TEST_F(MobileOperatorInfoMainTest, MNOByOperatorNameAndMCCMNC) {
  // message: Has MNOs with no MVNO.
  // match by: OperatorName finds two, MCCMNC narrows down to one.
  // verify: Observer event, fields.
  // This is merely an OperatorName update.
  ExpectEventCount(0);
  operator_info_->UpdateOperatorName("name107");
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("107002");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid107002");

  ResetOperatorInfo();
  // Try updates in reverse order.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("107001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid107001");
}

TEST_F(MobileOperatorInfoMainTest, MNOByMCCMNCOverridesOperatorName) {
  // message: Has MNOs with no MVNO.
  // match by: First MCCMNC finds one. Then, OperatorName matches another.
  // verify: MCCMNC match prevails. No change on OperatorName update.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("108001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid108001");

  // An event is sent for the updated OperatorName.
  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name108002");  // Does not match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid108001");
  EXPECT_EQ("name108002", operator_info_->operator_name());

  ResetOperatorInfo();
  // message: Same as above.
  // match by: First OperatorName finds one, then MCCMNC overrides it.
  // verify: Two events, MCCMNC one overriding the OperatorName one.
  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name108001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid108001");

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("108002");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid108002");
  // Name should remain unchanged though.
  EXPECT_EQ("name108001", operator_info_->operator_name());

  // message: Same as above.
  // match by: First a *wrong* MCCMNC update, followed by the correct Name
  // update.
  // verify: No MNO, since MCCMNC is given precedence.
  ResetOperatorInfo();
  ExpectEventCount(0);
  operator_info_->UpdateMCCMNC("108999");  // Does not match.
  operator_info_->UpdateOperatorName("name108001");
  VerifyEventCount();
  VerifyNoMatch();
}

TEST_F(MobileOperatorInfoMainTest, MNOByIMSI) {
  // message: Has MNO with no MVNO.
  // match by: MCCMNC part of IMSI of length 5 / 6.
  ExpectEventCount(0);
  operator_info_->UpdateIMSI("109");  // Too short.
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(0);
  operator_info_->UpdateIMSI("109995432154321");  // No match.
  VerifyEventCount();
  VerifyNoMatch();

  ResetOperatorInfo();
  // Short MCCMNC match.
  ExpectEventCount(1);
  operator_info_->UpdateIMSI("109015432154321");  // First 5 digits match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid10901");

  ResetOperatorInfo();
  // Long MCCMNC match.
  ExpectEventCount(1);
  operator_info_->UpdateIMSI("10900215432154321");  // First 6 digits match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid109002");
}

TEST_F(MobileOperatorInfoMainTest, MNOByMCCMNCOverridesIMSI) {
  // message: Has MNOs with no MVNO.
  // match by: One matches MCCMNC, then one matches a different MCCMNC substring
  //    of IMSI
  // verify: Observer event for the first match, all fields. Second Update
  // ignored.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("110001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid110001");

  // MNO remains unchanged on a mismatched IMSI update.
  ExpectEventCount(0);
  operator_info_->UpdateIMSI("1100025432154321");  // First 6 digits match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid110001");

  // MNO remains uncnaged on an invalid IMSI update.
  ExpectEventCount(0);
  operator_info_->UpdateIMSI("1100035432154321");  // Prefix does not match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid110001");

  ExpectEventCount(0);
  operator_info_->UpdateIMSI("110");  // Too small.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid110001");

  ResetOperatorInfo();
  // Same as above, but this time, match with IMSI, followed by a contradictory
  // MCCMNC update. The second update should override the first one.
  ExpectEventCount(1);
  operator_info_->UpdateIMSI("1100025432154321");  // First 6 digits match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid110002");

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("110001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid110001");
}

TEST_F(MobileOperatorInfoMainTest, MNOUchangedBySecondaryUpdates) {
  // This test verifies that only some updates affect the MNO.
  // message: Has MNOs with no MVNO.
  // match by: First matches the MCCMNC. Later, MNOs with a different MCCMNC
  //    matchs the given SID, NID, ICCID.
  // verify: Only one Observer event, on the first MCCMNC match.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("111001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid111001");

  ExpectEventCount(1);  // SID change event.
  operator_info_->UpdateSID("111102");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid111001");

  ExpectEventCount(1);  // NID change event.
  operator_info_->UpdateNID("111202");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid111001");
}

TEST_F(MobileOperatorInfoMainTest, MVNODefaultMatch) {
  // message: MNO with one MVNO (no filter).
  // match by: MNO matches by MCCMNC.
  // verify: Observer event for MVNO match. Uuid match the MVNO.
  // second update: ICCID.
  // verify: No observer event, match remains unchanged.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("112001");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid112002");

  ExpectEventCount(0);
  operator_info_->UpdateICCID("112002");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid112002");
}

TEST_F(MobileOperatorInfoMainTest, MVNONameMatch) {
  // message: MNO with one MVNO (name filter).
  // match by: MNO matches by MCCMNC,
  //           MVNO fails to match by fist name update,
  //           then MVNO matches by name.
  // verify: Two Observer events: MNO followed by MVNO.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("113001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid113001");

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name113999");  // No match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid113001");
  EXPECT_EQ("name113999", operator_info_->operator_name());

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name113002");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid113002");
  EXPECT_EQ("name113002", operator_info_->operator_name());
}

TEST_F(MobileOperatorInfoMainTest, MVNONameMalformedRegexMatch) {
  // message: MNO with one MVNO (name filter with a malformed regex).
  // match by: MNO matches by MCCMNC.
  //           MVNO does not match
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("114001");
  operator_info_->UpdateOperatorName("name[");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid114001");
}

TEST_F(MobileOperatorInfoMainTest, MVNONameSubexpressionRegexMatch) {
  // message: MNO with one MVNO (name filter with simple regex).
  // match by: MNO matches by MCCMNC.
  //           MVNO does not match with a name whose subexpression matches the
  //           regex.
  ExpectEventCount(2);  // One event for just the name update.
  operator_info_->UpdateMCCMNC("115001");
  operator_info_->UpdateOperatorName("name115_ExtraCrud");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid115001");

  ResetOperatorInfo();
  ExpectEventCount(2);  // One event for just the name update.
  operator_info_->UpdateMCCMNC("115001");
  operator_info_->UpdateOperatorName("ExtraCrud_name115");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid115001");

  ResetOperatorInfo();
  ExpectEventCount(2);  // One event for just the name update.
  operator_info_->UpdateMCCMNC("115001");
  operator_info_->UpdateOperatorName("ExtraCrud_name115_ExtraCrud");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid115001");

  ResetOperatorInfo();
  ExpectEventCount(2);  // One event for just the name update.
  operator_info_->UpdateMCCMNC("115001");
  operator_info_->UpdateOperatorName("name_ExtraCrud_115");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid115001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("115001");
  operator_info_->UpdateOperatorName("name115");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid115002");
}

TEST_F(MobileOperatorInfoMainTest, MVNONameRegexMatch) {
  // message: MNO with one MVNO (name filter with non-trivial regex).
  // match by: MNO matches by MCCMNC.
  //           MVNO fails to match several times with different strings.
  //           MVNO matches several times with different values.

  // Make sure we're not taking the regex literally!
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("116001");
  operator_info_->UpdateOperatorName("name[a-zA-Z_]*116[0-9]{0,3}");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid116001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("116001");
  operator_info_->UpdateOperatorName("name[a-zA-Z_]116[0-9]");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid116001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("116001");
  operator_info_->UpdateOperatorName("nameb*1167");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid116001");

  // Success!
  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("116001");
  operator_info_->UpdateOperatorName("name116");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid116002");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("116001");
  operator_info_->UpdateOperatorName("nameSomeWord116");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid116002");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("116001");
  operator_info_->UpdateOperatorName("name116567");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid116002");
}

TEST_F(MobileOperatorInfoMainTest, MVNONameMatchMultipleFilters) {
  // message: MNO with one MVNO with two name filters.
  // match by: MNO matches by MCCMNC.
  //           MVNO first fails on the second filter alone.
  //           MVNO fails on the first filter alone.
  //           MVNO matches on both filters.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("117001");
  operator_info_->UpdateOperatorName("nameA_crud");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid117001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("117001");
  operator_info_->UpdateOperatorName("crud_nameB");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid117001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("117001");
  operator_info_->UpdateOperatorName("crud_crud");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid117001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("117001");
  operator_info_->UpdateOperatorName("nameA_nameB");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid117002");
}

TEST_F(MobileOperatorInfoMainTest, MVNOIMSIMatch) {
  // message: MNO with one MVNO (imsi filter).
  // match by: MNO matches by MCCMNC,
  //           MVNO fails to match by fist imsi update,
  //           then MVNO matches by imsi.
  // verify: Two Observer events: MNO followed by MVNO.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("118001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid118001");

  ExpectEventCount(0);
  operator_info_->UpdateIMSI("1180011234512345");  // No match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid118001");

  ExpectEventCount(1);
  operator_info_->UpdateIMSI("1180015432154321");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid118002");
}

TEST_F(MobileOperatorInfoMainTest, MVNOICCIDMatch) {
  // message: MNO with one MVNO (iccid filter).
  // match by: MNO matches by MCCMNC,
  //           MVNO fails to match by fist iccid update,
  //           then MVNO matches by iccid.
  // verify: Two Observer events: MNO followed by MVNO.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("119001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid119001");

  ExpectEventCount(0);
  operator_info_->UpdateICCID("119987654321");  // No match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid119001");

  ExpectEventCount(1);
  operator_info_->UpdateICCID("119123456789");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid119002");
}

TEST_F(MobileOperatorInfoMainTest, MVNOSIDMatch) {
  // message: MNO with one MVNO (sid filter).
  // match by: MNO matches by MCCMNC,
  //           MVNO fails to match by fist sid update,
  //           then MVNO matches by sid.
  // verify: Two Observer events: MNO followed by MVNO.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("120001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid120001");

  ExpectEventCount(1);
  operator_info_->UpdateSID("120999");  // No match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid120001");
  EXPECT_EQ("120999", operator_info_->sid());

  ExpectEventCount(1);
  operator_info_->UpdateSID("120123");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid120002");
  EXPECT_EQ("120123", operator_info_->sid());
}

TEST_F(MobileOperatorInfoMainTest, MVNOAllMatch) {
  // message: MNO with following MVNOS:
  //   - one with no filter.
  //   - one with name filter.
  //   - one with imsi filter.
  //   - one with iccid filter.
  //   - one with name and iccid filter.
  // verify:
  //   - initial MCCMNC matches the default MVNO directly (not MNO)
  //   - match each of the MVNOs in turn.
  //   - give super set information that does not match any MVNO correctly,
  //     verify that the MNO matches.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("121001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid121001");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("121001");
  operator_info_->UpdateOperatorName("name121003");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid121003");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("121001");
  operator_info_->UpdateIMSI("1210045432154321");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid121004");

  ResetOperatorInfo();
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("121001");
  operator_info_->UpdateICCID("121005123456789");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid121005");

  ResetOperatorInfo();
  ExpectEventCount(3);
  operator_info_->UpdateMCCMNC("121001");
  operator_info_->UpdateOperatorName("name121006");
  VerifyMNOWithUUID("uuid121001");
  operator_info_->UpdateICCID("121006123456789");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid121006");
}

TEST_F(MobileOperatorInfoMainTest, MVNOMatchAndMismatch) {
  // message: MNO with one MVNO with name filter.
  // match by: MNO matches by MCCMNC
  //           MVNO matches by name.
  //           Second name update causes the MVNO to not match again.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("113001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid113001");

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name113002");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid113002");
  EXPECT_EQ("name113002", operator_info_->operator_name());

  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name113999");  // No match.
  VerifyEventCount();
  VerifyMNOWithUUID("uuid113001");
  EXPECT_EQ("name113999", operator_info_->operator_name());
}

TEST_F(MobileOperatorInfoMainTest, MVNOMatchAndReset) {
  // message: MVNO with name filter.
  // verify;
  //   - match MVNO by name.
  //   - Reset object, verify Observer event, and not match.
  //   - match MVNO by name again.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("113001");
  VerifyEventCount();
  ExpectEventCount(1);
  VerifyMNOWithUUID("uuid113001");
  operator_info_->UpdateOperatorName("name113002");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid113002");
  EXPECT_EQ("name113002", operator_info_->operator_name());

  ExpectEventCount(1);
  operator_info_->Reset();
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("113001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid113001");
  ExpectEventCount(1);
  operator_info_->UpdateOperatorName("name113002");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid113002");
  EXPECT_EQ("name113002", operator_info_->operator_name());
}

class MobileOperatorInfoDataTest : public MobileOperatorInfoMainTest {
 public:
   MobileOperatorInfoDataTest() : MobileOperatorInfoMainTest() {}

  // Same as MobileOperatorInfoMainTest, except that the database used is
  // different.
  virtual void SetUp() {
    operator_info_->ClearDatabasePaths();
    AddDatabase(mobile_operator_db::data_test,
                arraysize(mobile_operator_db::data_test));
    operator_info_->Init();
    operator_info_->AddObserver(&observer_);
  }

 protected:
  // This is a function that does a best effort verification of the information
  // that is obtained from the database by the MobileOperatorInfo object against
  // expectations stored in the form of data members in this class.
  // This is not a full proof check. In particular:
  //  - It is unspecified in some case which of the values from a list is
  //    exposed as a property. For example, at best, we can check that |sid| is
  //    non-empty.
  //  - It is not robust to "" as property values at times.
  void VerifyDatabaseData() {
    EXPECT_EQ(country_, operator_info_->country());
    EXPECT_EQ(requires_roaming_, operator_info_->requires_roaming());
    EXPECT_EQ(activation_code_, operator_info_->activation_code());

    EXPECT_EQ(mccmnc_list_.size(), operator_info_->mccmnc_list().size());
    set<string> mccmnc_set(operator_info_->mccmnc_list().begin(),
                           operator_info_->mccmnc_list().end());
    for (const auto &mccmnc : mccmnc_list_) {
      EXPECT_TRUE(mccmnc_set.find(mccmnc) != mccmnc_set.end());
    }
    if (mccmnc_list_.size() > 0) {
      // It is not specified which entry will be chosen, but mccmnc() must be
      // non empty.
      EXPECT_FALSE(operator_info_->mccmnc().empty());
    }

    VerifyNameListsMatch(operator_name_list_,
                         operator_info_->operator_name_list());

    // This comparison breaks if two apns have the same |apn| field.
    EXPECT_EQ(apn_list_.size(), operator_info_->apn_list().size());
    map<string, const MobileOperatorInfo::MobileAPN *> mobile_apns;
    for (const auto &apn_node : operator_info_->apn_list()) {
      mobile_apns[apn_node->apn] = apn_node;
    }
    for (const auto &apn_lhs : apn_list_) {
      ASSERT_TRUE(mobile_apns.find(apn_lhs->apn) != mobile_apns.end());
      const auto &apn_rhs = mobile_apns[apn_lhs->apn];
      // Only comparing apn, name, username, password.
      EXPECT_EQ(apn_lhs->apn, apn_rhs->apn);
      EXPECT_EQ(apn_lhs->username, apn_rhs->username);
      EXPECT_EQ(apn_lhs->password, apn_rhs->password);
      VerifyNameListsMatch(apn_lhs->operator_name_list,
                           apn_rhs->operator_name_list);
    }

    EXPECT_EQ(olp_list_.size(), operator_info_->olp_list().size());
    // This comparison breaks if two OLPs have the same |url|.
    map<string, MobileOperatorInfo::OnlinePortal> olps;
    for (const auto &olp : operator_info_->olp_list()) {
      olps[olp.url] = olp;
    }
    for (const auto &olp : olp_list_) {
      ASSERT_TRUE(olps.find(olp.url) != olps.end());
      const auto &olp_rhs = olps[olp.url];
      EXPECT_EQ(olp.method, olp_rhs.method);
      EXPECT_EQ(olp.post_data, olp_rhs.post_data);
    }

    EXPECT_EQ(sid_list_.size(), operator_info_->sid_list().size());
    set<string> sid_set(operator_info_->sid_list().begin(),
                        operator_info_->sid_list().end());
    for (const auto &sid : sid_list_) {
      EXPECT_TRUE(sid_set.find(sid) != sid_set.end());
    }
    if (sid_list_.size() > 0) {
      // It is not specified which entry will be chosen, but |sid()| must be
      // non-empty.
      EXPECT_FALSE(operator_info_->sid().empty());
    }
  }

  // This function does some extra checks for the user data that can not be done
  // when data is obtained from the database.
  void VerifyUserData() {
    EXPECT_EQ(sid_, operator_info_->sid());
  }

  void VerifyNameListsMatch(
      const vector<MobileOperatorInfo::LocalizedName> &operator_name_list_lhs,
      const vector<MobileOperatorInfo::LocalizedName> &operator_name_list_rhs) {
    // This comparison breaks if two localized names have the same |name|.
    map<string, MobileOperatorInfo::LocalizedName> localized_names;
    for (const auto &localized_name : operator_name_list_rhs) {
      localized_names[localized_name.name] = localized_name;
    }
    for (const auto &localized_name : operator_name_list_lhs) {
      EXPECT_TRUE(localized_names.find(localized_name.name) !=
                  localized_names.end());
      EXPECT_EQ(localized_name.language,
                localized_names[localized_name.name].language);
    }
  }

  // Use this function to pre-popluate all the data members of this object with
  // values matching the MNO for the database in |data_test.prototxt|.
  void PopulateMNOData() {
    country_ = "us";
    requires_roaming_ = true;
    activation_code_ = "open sesame";

    mccmnc_list_.clear();
    mccmnc_list_.push_back("200001");
    mccmnc_list_.push_back("200002");

    operator_name_list_.clear();
    operator_name_list_.push_back({"name200001", "en"});
    operator_name_list_.push_back({"name200002", ""});

    apn_list_.clear();
    MobileOperatorInfo::MobileAPN *apn;
    apn = new MobileOperatorInfo::MobileAPN();
    apn->apn = "test@test.com";
    apn->username = "testuser";
    apn->password = "is_public_boohoohoo";
    apn->operator_name_list.push_back({"name200003", "hi"});
    apn_list_.push_back(apn);  // Takes ownership.

    olp_list_.clear();
    olp_list_.push_back({"some@random.com", "POST", "random_data"});

    sid_list_.clear();
    sid_list_.push_back("200123");
    sid_list_.push_back("200234");
  }

  // Use this function to pre-populate all the data members of this object with
  // values matching the MVNO for the database in |data_test.prototext|.
  void PopulateMVNOData() {
    country_ = "ca";
    requires_roaming_ = false;
    activation_code_ = "khul ja sim sim";

    mccmnc_list_.clear();
    mccmnc_list_.push_back("200001");
    mccmnc_list_.push_back("200102");

    operator_name_list_.clear();
    operator_name_list_.push_back({"name200101", "en"});
    operator_name_list_.push_back({"name200102", ""});

    apn_list_.clear();
    MobileOperatorInfo::MobileAPN *apn;
    apn = new MobileOperatorInfo::MobileAPN();
    apn->apn = "test2@test.com";
    apn->username = "testuser2";
    apn->password = "is_public_boohoohoo_too";
    apn_list_.push_back(apn);  // Takes ownership.

    olp_list_.clear();
    olp_list_.push_back({"someother@random.com", "GET", ""});

    sid_list_.clear();
    sid_list_.push_back("200345");
  }

  // Data to be verified against the database.
  string country_;
  bool requires_roaming_;
  string activation_code_;
  vector<string> mccmnc_list_;
  vector<MobileOperatorInfo::LocalizedName> operator_name_list_;
  ScopedVector<MobileOperatorInfo::MobileAPN> apn_list_;
  vector<MobileOperatorInfo::OnlinePortal> olp_list_;
  vector<string> sid_list_;

  // Extra data to be verified only against user updates.
  string sid_;

  DISALLOW_COPY_AND_ASSIGN(MobileOperatorInfoDataTest);
};

TEST_F(MobileOperatorInfoDataTest, MNODetailedInformation) {
  // message: MNO with all the information filled in.
  // match by: MNO matches by MCCMNC
  // verify: All information is correctly loaded.
  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("200001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid200001");

  PopulateMNOData();
  VerifyDatabaseData();
}

TEST_F(MobileOperatorInfoDataTest, MVNOInheritsInformation) {
  // message: MVNO with name filter.
  // verify: All the missing fields are carried over to the MVNO from MNO.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200201");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200201");

  PopulateMNOData();
  VerifyDatabaseData();
}

TEST_F(MobileOperatorInfoDataTest, MVNOOverridesInformation) {
  // match by: MNO matches by MCCMNC, MVNO by name.
  // verify: All information is correctly loaded.
  //         The MVNO in this case overrides the information provided by MNO.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200101");

  PopulateMVNOData();
  VerifyDatabaseData();
}

TEST_F(MobileOperatorInfoDataTest, NoUpdatesBeforeMNOMatch) {
  // message: MVNO.
  // - do not match MNO with mccmnc/name
  // - on different updates, verify no events.
  ExpectEventCount(0);
  operator_info_->UpdateMCCMNC("200999");  // No match.
  operator_info_->UpdateOperatorName("name200001");  // matches MNO
  operator_info_->UpdateOperatorName("name200101");  // matches MVNO filter.
  operator_info_->UpdateSID("200123");  // Not used in any filter.
  VerifyEventCount();
  VerifyNoMatch();
}

TEST_F(MobileOperatorInfoDataTest, UserUpdatesOverrideMVNO) {
  // - match MVNO.
  // - send updates to properties and verify events are raised and values of
  //   updated properties override the ones provided by the database.
  string imsi {"2009991234512345"};
  string iccid {"200999123456789"};
  string sid {"200999"};
  string olp_url {"url@url.com"};
  string olp_method {"POST"};
  string olp_post_data {"data"};

  // Determine MVNO.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200101");

  // Send updates.
  ExpectEventCount(2);
  operator_info_->UpdateSID(sid);
  operator_info_->UpdateOnlinePortal(olp_url, olp_method, olp_post_data);
  operator_info_->UpdateIMSI(imsi);
  // No event raised because imsi is not exposed.
  operator_info_->UpdateICCID(iccid);
  // No event raised because ICCID is not exposed.

  VerifyEventCount();

  // Update our expectations.
  PopulateMVNOData();
  sid_ = sid;
  sid_list_.push_back(sid);
  olp_list_.push_back({olp_url, olp_method, olp_post_data});

  VerifyDatabaseData();
  VerifyUserData();
}

TEST_F(MobileOperatorInfoDataTest, CachedUserUpdatesOverrideMVNO) {
  // message: MVNO.
  // - First send updates that don't identify an MNO.
  // - Then identify an MNO and MVNO.
  // - verify that all the earlier updates are cached, and override the MVNO
  //   information.
  string imsi {"2009991234512345"};
  string iccid {"200999123456789"};
  string sid {"200999"};
  string olp_url {"url@url.com"};
  string olp_method {"POST"};
  string olp_post_data {"data"};

  // Send updates.
  ExpectEventCount(0);
  operator_info_->UpdateSID(sid);
  operator_info_->UpdateOnlinePortal(olp_url, olp_method, olp_post_data);
  operator_info_->UpdateIMSI(imsi);
  operator_info_->UpdateICCID(iccid);
  VerifyEventCount();

  // Determine MVNO.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200101");

  // Update our expectations.
  PopulateMVNOData();
  sid_ = sid;
  sid_list_.push_back(sid);
  olp_list_.push_back({olp_url, olp_method, olp_post_data});

  VerifyDatabaseData();
  VerifyUserData();
}

TEST_F(MobileOperatorInfoDataTest, RedundantUserUpdatesMVNO) {
  // - match MVNO.
  // - send redundant updates to properties.
  // - Verify no events, no updates to properties.

  // Identify MVNO.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200101");

  // Send redundant updates.
  // TODO(pprabhu)
  // Both the |UpdateSID| and |UpdateOnlinePortal| lead to an event because this
  // is the first time this values were set *by the user*. Although the values
  // from the database were the same, we did not use those values for filters.
  // It would be ideal to not raise these redundant events (since no public
  // information about the object changed), but I haven't invested in doing so
  // yet.
  ExpectEventCount(2);
  operator_info_->UpdateSID(operator_info_->sid());
  operator_info_->UpdateOperatorName(operator_info_->operator_name());
  operator_info_->UpdateOnlinePortal("someother@random.com", "GET", "");
  VerifyEventCount();
  PopulateMVNOData();
  VerifyDatabaseData();
}

TEST_F(MobileOperatorInfoDataTest, RedundantCachedUpdatesMVNO) {
  // message: MVNO.
  // - First send updates that don't identify MVNO, but match the data.
  // - Then idenityf an MNO and MVNO.
  // - verify that redundant information occurs only once.

  // Send redundant updates.
  ExpectEventCount(2);
  operator_info_->UpdateSID(operator_info_->sid());
  operator_info_->UpdateOperatorName(operator_info_->operator_name());
  operator_info_->UpdateOnlinePortal("someother@random.com", "GET", "");

  // Identify MVNO.
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200101");

  PopulateMVNOData();
  VerifyDatabaseData();
}

TEST_F(MobileOperatorInfoDataTest, ResetClearsInformation) {
  // Repeatedly reset the object and check M[V]NO identification and data.
  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200201");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200201");
  PopulateMNOData();
  VerifyDatabaseData();

  ExpectEventCount(1);
  operator_info_->Reset();
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyEventCount();
  VerifyMVNOWithUUID("uuid200101");
  PopulateMVNOData();
  VerifyDatabaseData();

  ExpectEventCount(1);
  operator_info_->Reset();
  VerifyEventCount();
  VerifyNoMatch();

  ExpectEventCount(1);
  operator_info_->UpdateMCCMNC("200001");
  VerifyEventCount();
  VerifyMNOWithUUID("uuid200001");
  PopulateMNOData();
  VerifyDatabaseData();
}

class MobileOperatorInfoObserverTest : public MobileOperatorInfoMainTest {
 public:
  MobileOperatorInfoObserverTest() : MobileOperatorInfoMainTest() {}

  // Same as |MobileOperatorInfoMainTest::SetUp|, except that we don't add a
  // default observer.
  virtual void SetUp() {
    operator_info_->ClearDatabasePaths();
    AddDatabase(mobile_operator_db::data_test,
                arraysize(mobile_operator_db::data_test));
    operator_info_->Init();
  }

 protected:
  // ///////////////////////////////////////////////////////////////////////////
  // Data.
  MockMobileOperatorInfoObserver second_observer_;

  DISALLOW_COPY_AND_ASSIGN(MobileOperatorInfoObserverTest);
};

TEST_F(MobileOperatorInfoObserverTest, NoObserver) {
  // - Don't add any observers, and then cause an MVNO update to occur.
  // - Verify no crash.
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
}

TEST_F(MobileOperatorInfoObserverTest, MultipleObservers) {
  // - Add two observers, and then cause an MVNO update to occur.
  // - Verify both observers are notified.
  operator_info_->AddObserver(&observer_);
  operator_info_->AddObserver(&second_observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(2);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyMVNOWithUUID("uuid200101");

  dispatcher_.DispatchPendingEvents();
}

TEST_F(MobileOperatorInfoObserverTest, LateObserver) {
  // - Add one observer, and verify it gets an MVNO update.
  operator_info_->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(2);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(0);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyMVNOWithUUID("uuid200101");
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&second_observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(1);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(0);
  operator_info_->Reset();
  VerifyNoMatch();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&second_observer_);

  // - Add another observer, verify both get an MVNO update.
  operator_info_->AddObserver(&second_observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(2);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyMVNOWithUUID("uuid200101");
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&second_observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(1);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(1);
  operator_info_->Reset();
  VerifyNoMatch();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&second_observer_);

  // - Remove an observer, verify it no longer gets updates.
  operator_info_->RemoveObserver(&observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(0);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(2);
  operator_info_->UpdateMCCMNC("200001");
  operator_info_->UpdateOperatorName("name200101");
  VerifyMVNOWithUUID("uuid200101");
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&second_observer_);

  EXPECT_CALL(observer_, OnOperatorChanged()).Times(0);
  EXPECT_CALL(second_observer_, OnOperatorChanged()).Times(1);
  operator_info_->Reset();
  VerifyNoMatch();
  dispatcher_.DispatchPendingEvents();
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(&second_observer_);
}

}  // namespace shill
