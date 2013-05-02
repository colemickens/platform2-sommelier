// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pkcs11_keystore.h"

#include <map>
#include <string>
#include <vector>

#include <base/string_number_conversions.h>
#include <chaps/attributes.h>
#include <chaps/chaps_proxy_mock.h>
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using chaps::Attributes;
using chromeos::SecureBlob;
using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StartsWith;

namespace cryptohome {

typedef chaps::ChapsProxyMock Pkcs11Mock;

const uint64_t kDefaultHandle = 7;  // Arbitrary non-zero value.

// Implements a fake PKCS #11 object store.  Labeled data blobs can be stored
// and later retrieved.  No handle management is performed, the emitted handles
// are always kDefaultHandle.  The mocked interface is ChapsInterface so these
// tests must be linked with the Chaps PKCS #11 library.  The mock class itself
// is part of the Chaps package; it is reused here to avoid duplication (see
// chaps_proxy_mock.h).
class KeyStoreTest : public testing::Test {
 public:
  KeyStoreTest()
      : pkcs11_(false),  // Do not pre-initialize the mock PKCS #11 library.
                         // This just controls whether the first call to
                         // C_Initialize returns 'already initialized'.
        find_status_(false) {}
  virtual ~KeyStoreTest() {}

  void SetUp() {
    ON_CALL(pkcs11_, OpenSession(_, 0, _, _))
        .WillByDefault(DoAll(SetArgumentPointee<3>(kDefaultHandle), Return(0)));
    ON_CALL(pkcs11_, CloseSession(_, kDefaultHandle))
        .WillByDefault(Return(0));
    ON_CALL(pkcs11_, CreateObject(_, kDefaultHandle, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::CreateObject));
    ON_CALL(pkcs11_, DestroyObject(_, kDefaultHandle, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::DestroyObject));
    ON_CALL(pkcs11_, GetAttributeValue(_, kDefaultHandle, kDefaultHandle, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::GetAttributeValue));
    ON_CALL(pkcs11_, SetAttributeValue(_, kDefaultHandle, kDefaultHandle, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::SetAttributeValue));
    ON_CALL(pkcs11_, FindObjectsInit(_, kDefaultHandle, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::FindObjectsInit));
    ON_CALL(pkcs11_, FindObjects(_, kDefaultHandle, 1, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::FindObjects));
    ON_CALL(pkcs11_, FindObjectsFinal(_, kDefaultHandle))
        .WillByDefault(Return(0));
  }

  // Stores a new labeled object, only CKA_LABEL and CKA_VALUE are relevant.
  virtual uint32_t CreateObject(const SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const vector<uint8_t>& attributes,
                                uint64_t* new_object_handle) {
    *new_object_handle = kDefaultHandle;
    objects_[GetValue(attributes, CKA_LABEL)] = GetValue(attributes, CKA_VALUE);
    return CKR_OK;
  }

  // Deletes a labeled object.
  virtual uint32_t DestroyObject(const SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 uint64_t object_handle) {
    objects_.erase(current_object_);
    return CKR_OK;
  }

  // Supports reading CKA_VALUE.
  virtual uint32_t GetAttributeValue(const SecureBlob& isolate_credential,
                                     uint64_t session_id,
                                     uint64_t object_handle,
                                     const vector<uint8_t>& attributes_in,
                                     vector<uint8_t>* attributes_out) {
    string value = objects_[current_object_];
    Attributes parsed;
    parsed.Parse(attributes_in);
    if (parsed.num_attributes() != 1 ||
        parsed.attributes()[0].type != CKA_VALUE ||
        (parsed.attributes()[0].pValue &&
         parsed.attributes()[0].ulValueLen != value.size()))
      return CKR_GENERAL_ERROR;
    parsed.attributes()[0].ulValueLen = value.size();
    if (parsed.attributes()[0].pValue)
      memcpy(parsed.attributes()[0].pValue, value.data(), value.size());
    parsed.Serialize(attributes_out);
    return CKR_OK;
  }

  // Supports writing CKA_VALUE.
  virtual uint32_t SetAttributeValue(
      const SecureBlob& isolate_credential,
      uint64_t session_id,
      uint64_t object_handle,
      const std::vector<uint8_t>& attributes) {
    objects_[current_object_] = GetValue(attributes, CKA_VALUE);
    return CKR_OK;
  }

  // Finds stored objects by CKA_LABEL.  Since we always use kDefaultHandle, the
  // only side-effect needs to be whether an object was found (find_status_).
  virtual uint32_t FindObjectsInit(const SecureBlob& isolate_credential,
                                   uint64_t session_id,
                                   const vector<uint8_t>& attributes) {
    string label = GetValue(attributes, CKA_LABEL);
    find_status_ = (objects_.find(label) != objects_.end());
    if (find_status_)
      current_object_ = label;
    return CKR_OK;
  }

  // Reports a 'found' object based on find_status_.
  virtual uint32_t FindObjects(const SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t max_object_count,
                               vector<uint64_t>* object_list) {
    if (find_status_)
      object_list->push_back(kDefaultHandle);
    find_status_ = false;
    return CKR_OK;
  }

 protected:
  NiceMock<Pkcs11Mock> pkcs11_;

  bool CompareBlob(const chromeos::SecureBlob& blob, const string& str) {
    string blob_str(reinterpret_cast<const char*>(blob.const_data()),
                    blob.size());
    return (blob_str == str);
  }

 private:
  map<string, string> objects_;  // The fake object store.
  string current_object_;  // The most recent object searched.
  bool find_status_;  // True if last search was successful.

  // A helper to pull the value for a given attribute out of a serialized
  // template.
  string GetValue(const vector<uint8_t>& attributes, CK_ATTRIBUTE_TYPE type) {
    Attributes parsed;
    parsed.Parse(attributes);
    CK_ATTRIBUTE_PTR array = parsed.attributes();
    for (CK_ULONG i = 0; i < parsed.num_attributes(); ++i) {
      if (array[i].type == type) {
        if (!array[i].pValue)
          return string();
        return string(reinterpret_cast<char*>(array[i].pValue),
                      array[i].ulValueLen);
      }
    }
    return string();
  }

  DISALLOW_COPY_AND_ASSIGN(KeyStoreTest);
};

// This test assumes that chaps in not available on the system running the test.
// The purpose of this test is to exercise the C_Initialize failure code path.
// Without a mock, the Chaps library will attempt to connect to the Chaps daemon
// unsuccessfully, resulting in a C_Initialize failure.
TEST(KeyStoreTest_NoMock, Pkcs11NotAvailable) {
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_FALSE(key_store.Read("test", &blob));
  EXPECT_FALSE(key_store.Write("test", blob));
}

// Exercises the key store when PKCS #11 returns success.  This exercises all
// non-error-handling code paths.
TEST_F(KeyStoreTest, Pkcs11Success) {
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_FALSE(key_store.Read("test", &blob));
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_TRUE(key_store.Read("test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data"));
  // Try with a different key name.
  EXPECT_FALSE(key_store.Read("test2", &blob));
  EXPECT_TRUE(key_store.Write("test2", SecureBlob("test_data2")));
  EXPECT_TRUE(key_store.Read("test2", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data2"));
  // Read the original key again.
  EXPECT_TRUE(key_store.Read("test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data"));
  // Replace key data.
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data3")));
  EXPECT_TRUE(key_store.Read("test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data3"));
  // Delete key data.
  EXPECT_TRUE(key_store.Delete("test2"));
  EXPECT_FALSE(key_store.Read("test2", &blob));
  EXPECT_TRUE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to open a session.
TEST_F(KeyStoreTest, NoSession) {
  EXPECT_CALL(pkcs11_, OpenSession(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_FALSE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to create an object.
TEST_F(KeyStoreTest, CreateObjectFail) {
  EXPECT_CALL(pkcs11_, CreateObject(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_FALSE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to read attribute values.
TEST_F(KeyStoreTest, ReadValueFail) {
  EXPECT_CALL(pkcs11_, GetAttributeValue(_, _, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to delete key data.
TEST_F(KeyStoreTest, DeleteValueFail) {
  EXPECT_CALL(pkcs11_, DestroyObject(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Write("test", SecureBlob("test_data2")));
  EXPECT_FALSE(key_store.Delete("test"));
}

// Tests the key store when PKCS #11 fails to find objects.  Tests each part of
// the multi-part find operation individually.
TEST_F(KeyStoreTest, FindFail) {
  EXPECT_CALL(pkcs11_, FindObjectsInit(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));

  EXPECT_CALL(pkcs11_, FindObjectsInit(_, _, _))
      .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(pkcs11_, FindObjects(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));

  EXPECT_CALL(pkcs11_, FindObjects(_, _, _, _))
      .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(pkcs11_, FindObjectsFinal(_, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 successfully finds zero objects.
TEST_F(KeyStoreTest, FindNoObjects) {
  vector<uint64_t> empty;
  EXPECT_CALL(pkcs11_, FindObjects(_, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(empty), Return(CKR_OK)));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

TEST_F(KeyStoreTest, Register) {
  Pkcs11KeyStore key_store;
  EXPECT_FALSE(key_store.Register(SecureBlob("private_key_blob"),
                                  SecureBlob("bad_pubkey")));
  const char* public_key_der_hex =
      "3082010A0282010100"
      "961037BC12D2A298BEBF06B2D5F8C9B64B832A2237F8CF27D5F96407A6041A4D"
      "AD383CB5F88E625F412E8ACD5E9D69DF0F4FA81FCE7955829A38366CBBA5A2B1"
      "CE3B48C14B59E9F094B51F0A39155874C8DE18A0C299EBF7A88114F806BE4F25"
      "3C29A509B10E4B19E31675AFE3B2DA77077D94F43D8CE61C205781ED04D183B4"
      "C349F61B1956C64B5398A3A98FAFF17D1B3D9120C832763EDFC8F4137F6EFBEF"
      "46D8F6DE03BD00E49DEF987C10BDD5B6F8758B6A855C23C982DDA14D8F0F2B74"
      "E6DEFA7EEE5A6FC717EB0FF103CB8049F693A2C8A5039EF1F5C025DC44BD8435"
      "E8D8375DADE00E0C0F5C196E04B8483CC98B1D5B03DCD7E0048B2AB343FFC11F"
      "0203"
      "010001";
  SecureBlob public_key_der;
  base::HexStringToBytes(public_key_der_hex, &public_key_der);
  EXPECT_TRUE(key_store.Register(SecureBlob("private_key_blob"),
                                 public_key_der));
}

}  // namespace cryptohome
