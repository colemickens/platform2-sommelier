// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_value_store.h"

#include <gtest/gtest.h>

using std::map;
using std::string;
using std::vector;
using testing::Test;

namespace shill {

class KeyValueStoreTest : public Test {
 public:
  KeyValueStoreTest() {}

 protected:
  KeyValueStore store_;
};

TEST_F(KeyValueStoreTest, Bool) {
  const string kKey("foo");
  const bool kDefaultValue = true;
  const bool kValue = false;
  EXPECT_FALSE(store_.ContainsBool(kKey));
  EXPECT_EQ(kDefaultValue, store_.LookupBool(kKey, kDefaultValue));
  store_.SetBool(kKey, kValue);
  EXPECT_TRUE(store_.ContainsBool(kKey));
  // TODO(shenhan): investigate if a newer version of gtest handles EXPECT_EQ
  // for bools in a manner that gcc 4.7 is happy with. (Improper conversion from
  // "false" to "NULL").
  EXPECT_EQ(static_cast<int>(kValue),
            static_cast<int>(store_.LookupBool(kKey, kDefaultValue)));
  EXPECT_EQ(static_cast<int>(kValue),
            static_cast<int>(store_.GetBool(kKey)));
}

TEST_F(KeyValueStoreTest, ByteArrays) {
  const string kKey("foo");
  const vector<vector<uint8_t>> kValue{ {1, 2, 3 } };
  EXPECT_FALSE(store_.ContainsByteArrays(kKey));
  store_.SetByteArrays(kKey, kValue);
  EXPECT_TRUE(store_.ContainsByteArrays(kKey));
  EXPECT_EQ(kValue, store_.GetByteArrays(kKey));
  store_.RemoveByteArrays(kKey);
  EXPECT_FALSE(store_.ContainsByteArrays(kKey));
}

TEST_F(KeyValueStoreTest, Int) {
  const string kKey("foo");
  const int kValue = 456;
  EXPECT_FALSE(store_.ContainsInt(kKey));
  const int kDefaultValue = 789;
  EXPECT_EQ(kDefaultValue, store_.LookupInt(kKey, kDefaultValue));
  store_.SetInt(kKey, kValue);
  EXPECT_TRUE(store_.ContainsInt(kKey));
  EXPECT_EQ(kValue, store_.GetInt(kKey));
  EXPECT_EQ(kValue, store_.LookupInt(kKey, kDefaultValue));
  store_.RemoveInt(kKey);
  EXPECT_FALSE(store_.ContainsInt(kKey));
}

TEST_F(KeyValueStoreTest, Int16) {
  const string kKey("foo");
  const int16_t kValue = 123;
  EXPECT_FALSE(store_.ContainsInt16(kKey));
  store_.SetInt16(kKey, kValue);
  EXPECT_TRUE(store_.ContainsInt16(kKey));
  EXPECT_EQ(kValue, store_.GetInt16(kKey));
  store_.RemoveInt16(kKey);
  EXPECT_FALSE(store_.ContainsInt16(kKey));
}

TEST_F(KeyValueStoreTest, KeyValueStore) {
  const string kSubKey("foo");
  const map<string, string> kSubValue{ { "bar0", "baz0" }, { "bar1", "baz1" } };
  KeyValueStore value;
  value.SetStringmap(kSubKey, kSubValue);
  const string kKey("foo");
  EXPECT_FALSE(store_.ContainsKeyValueStore(kKey));
  store_.SetKeyValueStore(kKey, value);
  EXPECT_TRUE(store_.ContainsKeyValueStore(kKey));
  EXPECT_TRUE(value.Equals(store_.GetKeyValueStore(kKey)));
  store_.RemoveKeyValueStore(kKey);
  EXPECT_FALSE(store_.ContainsKeyValueStore(kKey));
}

TEST_F(KeyValueStoreTest, RpcIdentifier) {
  const string kKey("foo");
  const string kValue("baz");
  EXPECT_FALSE(store_.ContainsRpcIdentifier(kKey));
  store_.SetRpcIdentifier(kKey, kValue);
  EXPECT_TRUE(store_.ContainsRpcIdentifier(kKey));
  EXPECT_EQ(kValue, store_.GetRpcIdentifier(kKey));
  store_.RemoveRpcIdentifier(kKey);
  EXPECT_FALSE(store_.ContainsRpcIdentifier(kKey));
}

TEST_F(KeyValueStoreTest, String) {
  const string kKey("foo");
  const string kDefaultValue("bar");
  const string kValue("baz");
  EXPECT_FALSE(store_.ContainsString(kKey));
  EXPECT_EQ(kDefaultValue, store_.LookupString(kKey, kDefaultValue));
  store_.SetString(kKey, kValue);
  EXPECT_TRUE(store_.ContainsString(kKey));
  EXPECT_EQ(kValue, store_.LookupString(kKey, kDefaultValue));
  EXPECT_EQ(kValue, store_.GetString(kKey));
  store_.RemoveString(kKey);
  EXPECT_FALSE(store_.ContainsString(kKey));
  EXPECT_EQ(kDefaultValue, store_.LookupString(kKey, kDefaultValue));
}

TEST_F(KeyValueStoreTest, Stringmap) {
  const string kKey("foo");
  const map<string, string> kValue{ { "bar0", "baz0" }, { "bar1", "baz1" } };
  EXPECT_FALSE(store_.ContainsStringmap(kKey));
  store_.SetStringmap(kKey, kValue);
  EXPECT_TRUE(store_.ContainsStringmap(kKey));
  EXPECT_EQ(kValue, store_.GetStringmap(kKey));
  store_.RemoveStringmap(kKey);
  EXPECT_FALSE(store_.ContainsStringmap(kKey));
}

TEST_F(KeyValueStoreTest, Strings) {
  const string kKey("foo");
  const vector<string> kValue{ "baz0", "baz1", "baz2" };
  EXPECT_FALSE(store_.ContainsStrings(kKey));
  store_.SetStrings(kKey, kValue);
  EXPECT_TRUE(store_.ContainsStrings(kKey));
  EXPECT_EQ(kValue, store_.GetStrings(kKey));
  store_.RemoveStrings(kKey);
  EXPECT_FALSE(store_.ContainsStrings(kKey));
}

TEST_F(KeyValueStoreTest, Uint) {
  const string kKey("foo");
  const uint32_t kValue = 456;
  EXPECT_FALSE(store_.ContainsUint(kKey));
  store_.SetUint(kKey, kValue);
  EXPECT_TRUE(store_.ContainsUint(kKey));
  EXPECT_EQ(kValue, store_.GetUint(kKey));
}

TEST_F(KeyValueStoreTest, Uint16) {
  const string kKey("foo");
  const uint16_t kValue = 456;
  EXPECT_FALSE(store_.ContainsUint16(kKey));
  store_.SetUint16(kKey, kValue);
  EXPECT_TRUE(store_.ContainsUint16(kKey));
  EXPECT_EQ(kValue, store_.GetUint16(kKey));
}

TEST_F(KeyValueStoreTest, Uint8s) {
  const string kKey("foo");
  const vector<uint8_t> kValue{ 1, 2, 3 };
  EXPECT_FALSE(store_.ContainsUint8s(kKey));
  store_.SetUint8s(kKey, kValue);
  EXPECT_TRUE(store_.ContainsUint8s(kKey));
  EXPECT_EQ(kValue, store_.GetUint8s(kKey));
  store_.RemoveUint8s(kKey);
  EXPECT_FALSE(store_.ContainsUint8s(kKey));
}

TEST_F(KeyValueStoreTest, Uint32s) {
  const string kKey("foo");
  const vector<uint32_t> kValue{ 1, 2, 3 };
  EXPECT_FALSE(store_.ContainsUint32s(kKey));
  store_.SetUint32s(kKey, kValue);
  EXPECT_TRUE(store_.ContainsUint32s(kKey));
  EXPECT_EQ(kValue, store_.GetUint32s(kKey));
  store_.RemoveUint32s(kKey);
  EXPECT_FALSE(store_.ContainsUint32s(kKey));
}

TEST_F(KeyValueStoreTest, DoubleRemove) {
  const string kKey("foo");
  // Make sure we don't get an exception/infinite loop if we do a
  // "Remove()" when the key does not exist.
  store_.RemoveInt(kKey);
  store_.RemoveInt(kKey);
  store_.RemoveString(kKey);
  store_.RemoveString(kKey);
}

TEST_F(KeyValueStoreTest, Clear) {
  EXPECT_TRUE(store_.IsEmpty());
  const string kBoolKey("foo");
  const bool kBoolValue = true;
  store_.SetBool(kBoolKey, kBoolValue);
  const string kByteArraysKey("bytearrays");
  const vector<vector<uint8_t>> kByteArraysValue{ {1, 2} };
  store_.SetByteArrays(kByteArraysKey, kByteArraysValue);
  const string kIntKey("bar");
  const int kIntValue = 123;
  store_.SetInt(kIntKey, kIntValue);
  const string kInt16Key("int16");
  const int16_t kInt16Value = 123;
  store_.SetInt16(kInt16Key, kInt16Value);
  const string kKeyValueStoreKey("bear");
  const KeyValueStore kKeyValueStoreValue;
  store_.SetKeyValueStore(kKeyValueStoreKey, kKeyValueStoreValue);
  const string kRpcIdentifierKey("rpcid");
  const string kRpcIdentifierValue("rpc_identifier");
  store_.SetRpcIdentifier(kRpcIdentifierKey, kRpcIdentifierValue);
  const string kStringKey("baz");
  const string kStringValue("string");
  store_.SetString(kStringKey, kStringValue);
  const string kStringmapKey("stringMapKey");
  const map<string, string> kStringmapValue;
  store_.SetStringmap(kStringmapKey, kStringmapValue);
  const string kStringsKey("stringsKey");
  const vector<string> kStringsValue;
  store_.SetStrings(kStringsKey, kStringsValue);
  const string kUintKey("bun");
  const uint32_t kUintValue = 456;
  store_.SetUint(kUintKey, kUintValue);
  const string kUint16Key("uint16");
  const uint16_t kUint16Value = 123;
  store_.SetUint16(kUint16Key, kUint16Value);
  const string kUint8sKey("uint8s");
  const vector<uint8_t> kUint8sValue{ 1, 2, 3 };
  store_.SetUint8s(kUint8sKey, kUint8sValue);
  const string kUint32sKey("uint32s");
  const vector<uint32_t> kUint32sValue{ 1, 2, 3 };
  store_.SetUint32s(kUint32sKey, kUint32sValue);

  EXPECT_TRUE(store_.ContainsBool(kBoolKey));
  EXPECT_TRUE(store_.ContainsByteArrays(kByteArraysKey));
  EXPECT_TRUE(store_.ContainsInt(kIntKey));
  EXPECT_TRUE(store_.ContainsInt16(kInt16Key));
  EXPECT_TRUE(store_.ContainsKeyValueStore(kKeyValueStoreKey));
  EXPECT_TRUE(store_.ContainsRpcIdentifier(kRpcIdentifierKey));
  EXPECT_TRUE(store_.ContainsString(kStringKey));
  EXPECT_TRUE(store_.ContainsStringmap(kStringmapKey));
  EXPECT_TRUE(store_.ContainsStrings(kStringsKey));
  EXPECT_TRUE(store_.ContainsUint(kUintKey));
  EXPECT_TRUE(store_.ContainsUint16(kUint16Key));
  EXPECT_TRUE(store_.ContainsUint8s(kUint8sKey));
  EXPECT_TRUE(store_.ContainsUint32s(kUint32sKey));
  EXPECT_FALSE(store_.IsEmpty());
  store_.Clear();
  EXPECT_TRUE(store_.IsEmpty());
  EXPECT_FALSE(store_.ContainsBool(kBoolKey));
  EXPECT_FALSE(store_.ContainsByteArrays(kByteArraysKey));
  EXPECT_FALSE(store_.ContainsInt(kIntKey));
  EXPECT_FALSE(store_.ContainsInt16(kInt16Key));
  EXPECT_FALSE(store_.ContainsInt(kKeyValueStoreKey));
  EXPECT_FALSE(store_.ContainsRpcIdentifier(kRpcIdentifierKey));
  EXPECT_FALSE(store_.ContainsString(kStringKey));
  EXPECT_FALSE(store_.ContainsStringmap(kStringmapKey));
  EXPECT_FALSE(store_.ContainsStrings(kStringsKey));
  EXPECT_FALSE(store_.ContainsUint(kUintKey));
  EXPECT_FALSE(store_.ContainsUint16(kUint16Key));
  EXPECT_FALSE(store_.ContainsUint8s(kUint8sKey));
  EXPECT_FALSE(store_.ContainsUint32s(kUint32sKey));
}

TEST_F(KeyValueStoreTest, Equals) {
  KeyValueStore first, second;

  first.SetBool("boolKey", true);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  second.SetBool("boolKey", true);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetBool("boolKey", true);
  second.SetBool("boolOtherKey", true);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetBool("boolKey", true);
  second.SetBool("boolKey", false);
  EXPECT_FALSE(first.Equals(second));

  const vector<vector<uint8_t>> kByteArrays1{ {1, 2} };
  const vector<vector<uint8_t>> kByteArrays2{ {3, 4} };

  first.Clear();
  second.Clear();
  first.SetByteArrays("byteArraysKey", kByteArrays1);
  second.SetByteArrays("byteArraysOtherKey", kByteArrays1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetByteArrays("byteArraysKey", kByteArrays1);
  second.SetByteArrays("byteArraysOtherKey", kByteArrays2);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetInt("intKey", 123);
  second.SetInt("intOtherKey", 123);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetInt("intKey", 123);
  second.SetInt("intKey", 456);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetInt16("int16Key", 123);
  second.SetInt16("int16OtherKey", 123);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetInt16("int16Key", 123);
  second.SetInt16("int16Key", 456);
  EXPECT_FALSE(first.Equals(second));

  KeyValueStore key_value0;
  key_value0.SetInt("intKey", 123);
  KeyValueStore key_value1;
  key_value1.SetInt("intOtherKey", 123);

  first.Clear();
  second.Clear();
  first.SetKeyValueStore("keyValueKey", key_value0);
  second.SetKeyValueStore("keyValueKey", key_value1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetKeyValueStore("keyValueKey", key_value0);
  second.SetKeyValueStore("keyValueOtherKey", key_value0);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetRpcIdentifier("rpcIdentifierKey", "rpcIdentifier");
  second.SetRpcIdentifier("rpcIdentifierOtherKey", "rpcIdentifier");
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetRpcIdentifier("rpcIdentifierKey", "rpcIdentifier");
  second.SetRpcIdentifier("rpcIdentifierKey", "otherRpcIdentifier");
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetString("stringKey", "string");
  second.SetString("stringOtherKey", "string");
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetString("stringKey", "string");
  second.SetString("stringKey", "otherString");
  EXPECT_FALSE(first.Equals(second));

  const map<string, string> kStringmap1{ { "key", "value" } };
  const map<string, string> kStringmap2{ { "otherKey", "value" } };
  const map<string, string> kStringmap3{ { "key", "otherValue" } };

  first.Clear();
  second.Clear();
  first.SetStringmap("stringmapKey", kStringmap1);
  second.SetStringmap("stringmapOtherKey", kStringmap1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetStringmap("stringmapKey", kStringmap1);
  second.SetStringmap("stringmapKey", kStringmap2);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetStringmap("stringmapKey", kStringmap1);
  second.SetStringmap("stringmapKey", kStringmap3);
  EXPECT_FALSE(first.Equals(second));

  const vector<string> kStrings1{ "value" };
  const vector<string> kStrings2{ "otherValue" };

  first.Clear();
  second.Clear();
  first.SetStrings("stringsKey", kStrings1);
  second.SetStrings("stringsOtherKey", kStrings1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetStrings("stringsKey", kStrings1);
  second.SetStrings("stringsKey", kStrings2);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetUint("uintKey", 1);
  second.SetUint("uintOtherKey", 1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetUint("uintKey", 1);
  second.SetUint("uintKey", 2);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetUint16("uint16Key", 1);
  second.SetUint16("uint16OtherKey", 1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetUint16("uint16Key", 1);
  second.SetUint16("uint16Key", 2);
  EXPECT_FALSE(first.Equals(second));

  const vector<uint8_t> kUint8s1{ 1 };
  const vector<uint8_t> kUint8s2{ 2 };

  first.Clear();
  second.Clear();
  first.SetUint8s("uint8sKey", kUint8s1);
  second.SetUint8s("uint8sOtherKey", kUint8s1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetUint8s("uint8sKey", kUint8s1);
  second.SetUint8s("uint8sKey", kUint8s2);
  EXPECT_FALSE(first.Equals(second));

  const vector<uint32_t> kUint32s1{ 1 };
  const vector<uint32_t> kUint32s2{ 2 };

  first.Clear();
  second.Clear();
  first.SetUint32s("uint32sKey", kUint32s1);
  second.SetUint32s("uint32sOtherKey", kUint32s1);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetUint32s("uint32sKey", kUint32s1);
  second.SetUint32s("uint32sKey", kUint32s2);
  EXPECT_FALSE(first.Equals(second));

  first.Clear();
  second.Clear();
  first.SetBool("boolKey", true);
  first.SetByteArrays("byteArraysKey", kByteArrays1);
  first.SetInt("intKey", 123);
  first.SetInt("int16Key", 123);
  first.SetRpcIdentifier("rpcIdentifierKey", "rpcid");
  first.SetString("stringKey", "value");
  first.SetStringmap("stringmapKey", kStringmap1);
  first.SetStrings("stringsKey", kStrings1);
  first.SetUint("uintKey", 1);
  first.SetUint16("uint16Key", 1);
  first.SetUint8s("uint8sKey", kUint8s1);
  first.SetUint32s("uint32sKey", kUint32s1);
  second.SetBool("boolKey", true);
  second.SetByteArrays("byteArraysKey", kByteArrays1);
  second.SetInt("intKey", 123);
  second.SetInt("int16Key", 123);
  second.SetRpcIdentifier("rpcIdentifierKey", "rpcid");
  second.SetString("stringKey", "value");
  second.SetStringmap("stringmapKey", kStringmap1);
  second.SetStrings("stringsKey", kStrings1);
  second.SetUint("uintKey", 1);
  second.SetUint16("uint16Key", 1);
  second.SetUint8s("uint8sKey", kUint8s1);
  second.SetUint32s("uint32sKey", kUint32s1);
  EXPECT_TRUE(first.Equals(second));
}

TEST_F(KeyValueStoreTest, CopyFrom) {
  KeyValueStore donor;
  const string kBoolKey("foo");
  const bool kBoolValue = true;
  donor.SetBool(kBoolKey, kBoolValue);
  const string kByteArraysKey("bytearrays");
  const vector<vector<uint8_t>> kByteArraysValue{ {1} };
  donor.SetByteArrays(kByteArraysKey, kByteArraysValue);
  const string kIntKey("bar");
  const int kIntValue = 123;
  donor.SetInt(kIntKey, kIntValue);
  const string kInt16Key("int16");
  const int16_t kInt16Value = 123;
  donor.SetInt16(kInt16Key, kInt16Value);
  const string kKeyValueStoreKey("bear");
  KeyValueStore keyValueStoreValue;
  keyValueStoreValue.SetInt(kIntKey, kIntValue);
  donor.SetKeyValueStore(kKeyValueStoreKey, keyValueStoreValue);
  const string kRpcIdentifierKey("rpcidentifier");
  const string kRpcIdentifierValue("rpcid");
  donor.SetRpcIdentifier(kRpcIdentifierKey, kRpcIdentifierValue);
  const string kStringKey("baz");
  const string kStringValue("string");
  donor.SetString(kStringKey, kStringValue);
  const string kStringmapKey("stringMapKey");
  const map<string, string> kStringmapValue{ { "key", "value" } };
  donor.SetStringmap(kStringmapKey, kStringmapValue);
  const string kStringsKey("stringsKey");
  const vector<string> kStringsValue{ "string0", "string1" };
  donor.SetStrings(kStringsKey, kStringsValue);
  const string kUintKey("bun");
  const uint32_t kUintValue = 456;
  donor.SetUint(kUintKey, kUintValue);
  const string kUint16Key("uint16");
  const uint16_t kUint16Value = 456;
  donor.SetUint16(kUint16Key, kUint16Value);
  const string kUint8sKey("uint8s");
  const vector<uint8_t> kUint8sValue{ 1 };
  donor.SetUint8s(kUint8sKey, kUint8sValue);
  const string kUint32sKey("uint32s");
  const vector<uint32_t> kUint32sValue{ 1 };
  donor.SetUint32s(kUint32sKey, kUint32sValue);

  EXPECT_TRUE(store_.IsEmpty());
  store_.CopyFrom(donor);
  EXPECT_FALSE(store_.IsEmpty());
  EXPECT_TRUE(donor.Equals(store_));
}

}  // namespace shill
