// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipp/ipp_attribute.h"

#include <limits>

#include <gtest/gtest.h>

namespace ipp {

static bool operator==(const ipp::RangeOfInteger& a,
                       const ipp::RangeOfInteger& b) {
  return (a.min_value == b.min_value && a.max_value == b.max_value);
}

namespace {

void TestNewAttribute(Attribute* attr,
                      const std::string& name,
                      bool is_a_set,
                      AttrType type) {
  EXPECT_TRUE(attr != nullptr);
  EXPECT_EQ(attr->GetName(), name);
  if (attr->GetNameAsEnum() != AttrName::_unknown) {
    EXPECT_EQ(ToString(attr->GetNameAsEnum()), name);
  }
  EXPECT_EQ(attr->IsASet(), is_a_set);
  EXPECT_EQ(attr->GetType(), type);
  // default state after creation
  EXPECT_EQ(attr->GetState(), AttrState::unset);
  EXPECT_EQ(attr->GetSize(), 0);
}

class TestSubcollection : public Collection {
 public:
  SingleValue<bool> hi{this, AttrName::job_id};
  TestSubcollection() : Collection(&defs_) {}

 private:
  std::vector<Attribute*> GetKnownAttributes() override { return {&hi}; }
  static const std::map<AttrName, AttrDef> defs_;
};
const std::map<AttrName, AttrDef> TestSubcollection::defs_ = {
    {AttrName::job_id, {AttrType::boolean, InternalType::kInteger, false}}};

struct TestCollection : public Collection {
  SingleValue<int> attr1{this, AttrName::attributes_charset};
  SetOfValues<RangeOfInteger> attr2{this, AttrName::auth_info};
  OpenSetOfValues<int> attr3{this, AttrName::baling};
  SingleCollection<TestSubcollection> attr4{
      this, AttrName::printer_supply_description};
  SetOfCollections<TestSubcollection> attr5{this, AttrName::punching_locations};
  std::vector<Attribute*> GetKnownAttributes() override {
    return {&attr1, &attr2, &attr3, &attr4, &attr5};
  }
  static const std::map<AttrName, AttrDef> defs_;
  TestCollection() : Collection(&defs_) {}
};
const std::map<AttrName, AttrDef> TestCollection::defs_ = {
    {AttrName::attributes_charset,
     {AttrType::integer, InternalType::kInteger, false}},
    {AttrName::auth_info,
     {AttrType::rangeOfInteger, InternalType::kRangeOfInteger, true}},
    {AttrName::baling, {AttrType::integer, InternalType::kString, true}},
    {AttrName::printer_supply_description,
     {AttrType::collection, InternalType::kCollection, false,
      []() -> Collection* { return new TestSubcollection(); }}},
    {AttrName::punching_locations,
     {AttrType::collection, InternalType::kCollection, true,
      []() -> Collection* { return new TestSubcollection(); }}}};

TEST(attribute, SingleValue) {
  TestCollection coll;
  TestNewAttribute(&coll.attr1, "attributes-charset", false, AttrType::integer);
  coll.attr1.Set(123);
  EXPECT_EQ(coll.attr1.Get(), 123);
  EXPECT_EQ(coll.attr1.GetState(), AttrState::set);
}

TEST(attribute, SetOfValues) {
  TestCollection coll;
  TestNewAttribute(&coll.attr2, "auth-info", true, AttrType::rangeOfInteger);
  RangeOfInteger r1{123, 234};
  RangeOfInteger r2{123, 234};
  RangeOfInteger r3{123, 234};
  coll.attr2.Set({r1, r2, r3});
  EXPECT_EQ(coll.attr2.Get(), std::vector<RangeOfInteger>({r1, r2, r3}));
  EXPECT_EQ(coll.attr2.GetState(), AttrState::set);
  EXPECT_EQ(coll.attr2.GetSize(), 3);
  coll.attr2.Resize(2);
  EXPECT_EQ(coll.attr2.Get(), std::vector<RangeOfInteger>({r1, r2}));
  EXPECT_EQ(coll.attr2.GetSize(), 2);
}

TEST(attribute, OpenSetOfValues) {
  TestCollection coll;
  TestNewAttribute(&coll.attr3, "baling", true, AttrType::integer);
  coll.attr3.Set({11, 22, 33});
  coll.attr3.Add(std::vector<std::string>({"aaa", "bbb"}));
  coll.attr3.Add({44, 55});
  EXPECT_EQ(coll.attr3.Get(), std::vector<std::string>({"11", "22", "33", "aaa",
                                                        "bbb", "44", "55"}));
  EXPECT_EQ(coll.attr3.GetState(), AttrState::set);
  EXPECT_EQ(coll.attr3.GetSize(), 7);
  coll.attr3.Resize(2);
  EXPECT_EQ(coll.attr3.Get(), std::vector<std::string>({"11", "22"}));
  EXPECT_EQ(coll.attr3.GetSize(), 2);
  coll.attr3.Set(std::vector<std::string>({"xx", "yy", "zz"}));
  EXPECT_EQ(coll.attr3.GetSize(), 3);
  EXPECT_EQ(coll.attr3.Get(), std::vector<std::string>({"xx", "yy", "zz"}));
}

TEST(attribute, SingleCollection) {
  TestCollection coll;
  TestNewAttribute(&coll.attr4, "printer-supply-description", false,
                   AttrType::collection);
  EXPECT_EQ(coll.attr4->hi.GetState(), AttrState::unset);
  EXPECT_EQ(coll.attr4.GetState(), AttrState::set);
  coll.attr4->hi.Set(false);
  EXPECT_EQ(coll.attr4->hi.Get(), false);
  EXPECT_EQ(coll.attr4->hi.GetState(), AttrState::set);
}

TEST(attribute, SetOfCollections) {
  TestCollection coll;
  TestNewAttribute(&coll.attr5, "punching-locations", true,
                   AttrType::collection);
  coll.attr5[3].hi.SetState(AttrState::not_settable);
  EXPECT_EQ(coll.attr5.GetState(), AttrState::set);
  EXPECT_EQ(coll.attr5.GetSize(), 4);
  EXPECT_EQ(coll.attr5[3].hi.GetState(), AttrState::not_settable);
}

TEST(attribute, UnknownValueAttribute) {
  TestCollection coll;
  Attribute* attr = coll.AddUnknownAttribute("abc", false, AttrType::name);
  TestNewAttribute(attr, "abc", false, AttrType::name);
  ASSERT_TRUE(attr->SetValue("val"));
  StringWithLanguage sl;
  ASSERT_TRUE(attr->GetValue(&sl));
  EXPECT_EQ(sl.language, "");
  EXPECT_EQ(sl.value, "val");
}

TEST(attribute, UnknownCollectionAttribute) {
  TestCollection coll;
  Attribute* attr =
      coll.AddUnknownAttribute("abcd", true, AttrType::collection);
  TestNewAttribute(attr, "abcd", true, AttrType::collection);
  EXPECT_EQ(attr->GetCollection(), nullptr);
  attr->Resize(3);
  EXPECT_NE(attr->GetCollection(), nullptr);
  EXPECT_NE(attr->GetCollection(2), nullptr);
  EXPECT_EQ(attr->GetCollection(3), nullptr);
}

TEST(attribute, FromStringToInt) {
  int val = 123456;
  // incorrect values: return false, no changes to val
  EXPECT_FALSE(FromString("123", static_cast<int*>(nullptr)));
  EXPECT_FALSE(FromString("12341s", &val));
  EXPECT_EQ(123456, val);
  EXPECT_FALSE(FromString("-", &val));
  EXPECT_EQ(123456, val);
  EXPECT_FALSE(FromString("", &val));
  EXPECT_EQ(123456, val);
  // correct values: return true
  EXPECT_TRUE(FromString("-239874", &val));
  EXPECT_EQ(-239874, val);
  EXPECT_TRUE(FromString("9238", &val));
  EXPECT_EQ(9238, val);
  EXPECT_TRUE(FromString("0", &val));
  EXPECT_EQ(0, val);
  const int int_min = std::numeric_limits<int>::min();
  const int int_max = std::numeric_limits<int>::max();
  EXPECT_TRUE(FromString(ToString(int_min), &val));
  EXPECT_EQ(int_min, val);
  EXPECT_TRUE(FromString(ToString(int_max), &val));
  EXPECT_EQ(int_max, val);
}

}  // end of namespace

}  // end of namespace ipp
