// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp/ipp_package.h"

#include <gtest/gtest.h>

#include "libipp/ipp_attribute.h"
#include "libipp/ipp_enums.h"

namespace ipp {

namespace {

struct TestSubcollection : public Collection {
  SingleValue<int> attr{this, AttrName::auth_info};
  std::vector<Attribute*> GetKnownAttributes() override { return {&attr}; }
  std::vector<const Attribute*> GetKnownAttributes() const override {
    return {&attr};
  }
  static const std::map<AttrName, AttrDef> defs_;
  TestSubcollection() : Collection(&defs_) {}
};
const std::map<AttrName, AttrDef> TestSubcollection::defs_ = {
    {AttrName::auth_info, {AttrType::integer, InternalType::kInteger, false}}};

struct TestCollection : public Collection {
  SingleValue<int> attr_single_val{this, AttrName::job_id};
  SetOfValues<int> attr_set_of_val{this, AttrName::job_name};
  SingleCollection<TestSubcollection> attr_single_coll{this,
                                                       AttrName::printer_info};
  SetOfCollections<TestSubcollection> attr_set_of_coll{
      this, AttrName::printer_supply};
  std::vector<Attribute*> GetKnownAttributes() override {
    return {&attr_single_val, &attr_set_of_val, &attr_single_coll,
            &attr_set_of_coll};
  }
  std::vector<const Attribute*> GetKnownAttributes() const override {
    return {&attr_single_val, &attr_set_of_val, &attr_single_coll,
            &attr_set_of_coll};
  }
  static const std::map<AttrName, AttrDef> defs_;
  TestCollection() : Collection(&defs_) {}
};
const std::map<AttrName, AttrDef> TestCollection::defs_ = {
    {AttrName::job_id, {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::job_name, {AttrType::integer, InternalType::kInteger, true}},
    {AttrName::printer_info,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new TestSubcollection(); }}},
    {AttrName::printer_supply,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new TestSubcollection(); }}}};

struct TestPackage : public Package {
  SingleGroup<TestCollection> grp_single{GroupTag::operation_attributes};
  SetOfGroups<TestCollection> grp_set{GroupTag::printer_attributes};
  std::vector<Group*> GetKnownGroups() override {
    return {&grp_single, &grp_set};
  }
  std::vector<const Group*> GetKnownGroups() const override {
    return {&grp_single, &grp_set};
  }
};

TEST(package, Collection) {
  TestCollection coll;
  // add attribute - name exists
  EXPECT_EQ(coll.AddUnknownAttribute("job-name", true, AttrType::integer),
            nullptr);
  EXPECT_EQ(coll.AddUnknownAttribute("job-name", false, AttrType::collection),
            nullptr);
  // add attribute - incorrect name
  EXPECT_EQ(coll.AddUnknownAttribute("", true, AttrType::boolean), nullptr);
  // add attribute
  Attribute* new_attr =
      coll.AddUnknownAttribute("other-name", true, AttrType::boolean);
  ASSERT_NE(new_attr, nullptr);
  EXPECT_EQ(new_attr->GetName(), "other-name");
  EXPECT_EQ(new_attr->GetType(), AttrType::boolean);
  // get known/all attributes
  std::vector<Attribute*> all = coll.GetAllAttributes();
  for (auto a : all)
    ASSERT_NE(a, nullptr);
  ASSERT_EQ(all.size(), 5);
  EXPECT_EQ(all[4], new_attr);
  std::vector<Attribute*> known = coll.GetKnownAttributes();
  all.pop_back();
  EXPECT_EQ(known, all);
  // get attribute by name
  EXPECT_EQ(coll.GetAttribute("printer-info"), &(coll.attr_single_coll));
  EXPECT_EQ(coll.GetAttribute(AttrName::job_name), &(coll.attr_set_of_val));
  EXPECT_EQ(coll.GetAttribute("other-name"), new_attr);
  EXPECT_EQ(coll.GetAttribute("adasad"), nullptr);
}

TEST(package, Package) {
  TestPackage pkg;
  // add new group - the same tag
  EXPECT_EQ(pkg.AddUnknownGroup(GroupTag::printer_attributes, false), nullptr);
  EXPECT_EQ(pkg.AddUnknownGroup(GroupTag::printer_attributes, true), nullptr);
  Group* new_grp = pkg.AddUnknownGroup(GroupTag::job_attributes, true);
  ASSERT_NE(new_grp, nullptr);
  EXPECT_EQ(new_grp->GetName(), GroupTag::job_attributes);
  EXPECT_EQ(new_grp->IsASet(), true);
  // get known/all groups
  std::vector<Group*> all = pkg.GetAllGroups();
  for (auto a : all)
    ASSERT_NE(a, nullptr);
  ASSERT_EQ(all.size(), 3);
  EXPECT_EQ(all.back(), new_grp);
  std::vector<Group*> known = pkg.GetKnownGroups();
  all.pop_back();
  EXPECT_EQ(known, all);
  // get group by name
  EXPECT_EQ(pkg.GetGroup(GroupTag::operation_attributes), &(pkg.grp_single));
  EXPECT_EQ(pkg.GetGroup(GroupTag::printer_attributes), &(pkg.grp_set));
  EXPECT_EQ(pkg.GetGroup(GroupTag::job_attributes), new_grp);
  EXPECT_EQ(pkg.GetGroup(GroupTag::subscription_attributes), nullptr);
}

}  // end of namespace

}  // end of namespace ipp
