// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pkcs11_keystore.h"

#include <map>
#include <string>
#include <vector>

#include <base/strings/string_number_conversions.h>
#include <chaps/attributes.h>
#include <chaps/chaps_proxy_mock.h>
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "make_tests.h"
#include "mock_pkcs11_init.h"

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

const uint64_t kSession = 7;  // Arbitrary non-zero value.
const char* kDefaultUser = "test_user";

// Implements a fake PKCS #11 object store.  Labeled data blobs can be stored
// and later retrieved.  The mocked interface is ChapsInterface so these
// tests must be linked with the Chaps PKCS #11 library.  The mock class itself
// is part of the Chaps package; it is reused here to avoid duplication (see
// chaps_proxy_mock.h).
class KeyStoreTest : public testing::Test {
 public:
  KeyStoreTest()
      : pkcs11_(false),  // Do not pre-initialize the mock PKCS #11 library.
                         // This just controls whether the first call to
                         // C_Initialize returns 'already initialized'.
        next_handle_(1) {}
  virtual ~KeyStoreTest() {}

  void SetUp() {
    helper_.SetUpSystemSalt();
    ON_CALL(pkcs11_, OpenSession(_, 0, _, _))
        .WillByDefault(DoAll(SetArgumentPointee<3>(kSession), Return(0)));
    ON_CALL(pkcs11_, CloseSession(_, _))
        .WillByDefault(Return(0));
    ON_CALL(pkcs11_, CreateObject(_, _, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::CreateObject));
    ON_CALL(pkcs11_, DestroyObject(_, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::DestroyObject));
    ON_CALL(pkcs11_, GetAttributeValue(_, _, _, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::GetAttributeValue));
    ON_CALL(pkcs11_, SetAttributeValue(_, _, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::SetAttributeValue));
    ON_CALL(pkcs11_, FindObjectsInit(_, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::FindObjectsInit));
    ON_CALL(pkcs11_, FindObjects(_, _, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::FindObjects));
    ON_CALL(pkcs11_, FindObjectsFinal(_, _))
        .WillByDefault(Return(0));
    ON_CALL(pkcs11_init_, GetTpmTokenSlotForPath(_, _))
        .WillByDefault(DoAll(SetArgumentPointee<1>(0), Return(true)));
  }

  void TearDown() {
    helper_.TearDownSystemSalt();
  }

  // Stores a new labeled object, only CKA_LABEL and CKA_VALUE are relevant.
  virtual uint32_t CreateObject(const SecureBlob& isolate_credential,
                                uint64_t session_id,
                                const vector<uint8_t>& attributes,
                                uint64_t* new_object_handle) {
    *new_object_handle = next_handle_++;
    string label = GetValue(attributes, CKA_LABEL);
    handles_[*new_object_handle] = label;
    values_[label] = GetValue(attributes, CKA_VALUE);
    labels_[label] = *new_object_handle;
    return CKR_OK;
  }

  // Deletes a labeled object.
  virtual uint32_t DestroyObject(const SecureBlob& isolate_credential,
                                 uint64_t session_id,
                                 uint64_t object_handle) {
    string label = handles_[object_handle];
    handles_.erase(object_handle);
    values_.erase(label);
    labels_.erase(label);
    return CKR_OK;
  }

  // Supports reading CKA_VALUE.
  virtual uint32_t GetAttributeValue(const SecureBlob& isolate_credential,
                                     uint64_t session_id,
                                     uint64_t object_handle,
                                     const vector<uint8_t>& attributes_in,
                                     vector<uint8_t>* attributes_out) {
    string label = handles_[object_handle];
    string value = values_[label];
    Attributes parsed;
    parsed.Parse(attributes_in);
    if (parsed.num_attributes() == 1 &&
        parsed.attributes()[0].type == CKA_LABEL)
      value = label;
    if (parsed.num_attributes() != 1 ||
        (parsed.attributes()[0].type != CKA_VALUE &&
         parsed.attributes()[0].type != CKA_LABEL) ||
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
    values_[handles_[object_handle]] = GetValue(attributes, CKA_VALUE);
    return CKR_OK;
  }

  // Finds stored objects by CKA_LABEL.  If no CKA_LABEL find all objects.
  virtual uint32_t FindObjectsInit(const SecureBlob& isolate_credential,
                                   uint64_t session_id,
                                   const vector<uint8_t>& attributes) {
    string label = GetValue(attributes, CKA_LABEL);
    found_objects_.clear();
    if (label.empty()) {
      for (map<uint64_t, string>::iterator it = handles_.begin();
           it != handles_.end();
           ++it) {
        found_objects_.push_back(it->first);
      }
    } else if (labels_.count(label) > 0) {
      found_objects_.push_back(labels_[label]);
    }
    return CKR_OK;
  }

  // Reports a 'found' object based on find_status_.
  virtual uint32_t FindObjects(const SecureBlob& isolate_credential,
                               uint64_t session_id,
                               uint64_t max_object_count,
                               vector<uint64_t>* object_list) {
    while (!found_objects_.empty() && object_list->size() < max_object_count) {
      object_list->push_back(found_objects_.back());
      found_objects_.pop_back();
    }
    return CKR_OK;
  }

 protected:
  NiceMock<Pkcs11Mock> pkcs11_;
  NiceMock<MockPkcs11Init> pkcs11_init_;

  bool CompareBlob(const chromeos::SecureBlob& blob, const string& str) {
    string blob_str(reinterpret_cast<const char*>(blob.const_data()),
                    blob.size());
    return (blob_str == str);
  }

 private:
  MakeTests helper_;
  map<string, string> values_;      // The fake object store: label->value
  map<uint64_t, string> handles_;   // The fake object store: handle->label
  map<string, uint64_t> labels_;    // The fake object store: label->handle
  vector<uint64_t> found_objects_;  // The most recent objects searched.
  uint64_t next_handle_;            // Tracks handle assignment.

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
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
  EXPECT_FALSE(key_store.Write(kDefaultUser, "test", blob));
}

// Exercises the key store when PKCS #11 returns success.  This exercises all
// non-error-handling code paths.
TEST_F(KeyStoreTest, Pkcs11Success) {
  Pkcs11KeyStore key_store(&pkcs11_init_);
  SecureBlob blob;
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data"));
  // Try with a different key name.
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test2", &blob));
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test2", SecureBlob("test_data2")));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "test2", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data2"));
  // Read the original key again.
  EXPECT_TRUE(key_store.Read(kDefaultUser, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data"));
  // Replace key data.
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data3")));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "test_data3"));
  // Delete key data.
  EXPECT_TRUE(key_store.Delete(kDefaultUser, "test2"));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test2", &blob));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "test", &blob));
}

// Tests the key store when PKCS #11 fails to open a session.
TEST_F(KeyStoreTest, NoSession) {
  EXPECT_CALL(pkcs11_, OpenSession(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store(&pkcs11_init_);
  SecureBlob blob;
  EXPECT_FALSE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
}

// Tests the key store when PKCS #11 fails to create an object.
TEST_F(KeyStoreTest, CreateObjectFail) {
  EXPECT_CALL(pkcs11_, CreateObject(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store(&pkcs11_init_);
  SecureBlob blob;
  EXPECT_FALSE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
}

// Tests the key store when PKCS #11 fails to read attribute values.
TEST_F(KeyStoreTest, ReadValueFail) {
  EXPECT_CALL(pkcs11_, GetAttributeValue(_, _, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store(&pkcs11_init_);
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
}

// Tests the key store when PKCS #11 fails to delete key data.
TEST_F(KeyStoreTest, DeleteValueFail) {
  EXPECT_CALL(pkcs11_, DestroyObject(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store(&pkcs11_init_);
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data2")));
  EXPECT_FALSE(key_store.Delete(kDefaultUser, "test"));
}

// Tests the key store when PKCS #11 fails to find objects.  Tests each part of
// the multi-part find operation individually.
TEST_F(KeyStoreTest, FindFail) {
  EXPECT_CALL(pkcs11_, FindObjectsInit(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store(&pkcs11_init_);
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));

  EXPECT_CALL(pkcs11_, FindObjectsInit(_, _, _))
      .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(pkcs11_, FindObjects(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));

  EXPECT_CALL(pkcs11_, FindObjects(_, _, _, _))
      .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(pkcs11_, FindObjectsFinal(_, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
}

// Tests the key store when PKCS #11 successfully finds zero objects.
TEST_F(KeyStoreTest, FindNoObjects) {
  vector<uint64_t> empty;
  EXPECT_CALL(pkcs11_, FindObjects(_, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(empty), Return(CKR_OK)));
  Pkcs11KeyStore key_store(&pkcs11_init_);
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write(kDefaultUser, "test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "test", &blob));
}

TEST_F(KeyStoreTest, Register) {
  Pkcs11KeyStore key_store(&pkcs11_init_);
  EXPECT_FALSE(key_store.Register(kDefaultUser,
                                  SecureBlob("private_key_blob"),
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
  EXPECT_TRUE(key_store.Register(kDefaultUser,
                                 SecureBlob("private_key_blob"),
                                 public_key_der));
}

// Tests that the DeleteByPrefix() method removes the correct objects and only
// the correct objects.
TEST_F(KeyStoreTest, DeleteByPrefix) {
  Pkcs11KeyStore key_store(&pkcs11_init_);

  // Test with no keys.
  ASSERT_TRUE(key_store.DeleteByPrefix(kDefaultUser, "prefix"));

  // Test with a single matching key.
  ASSERT_TRUE(key_store.Write(kDefaultUser, "prefix_test", SecureBlob("test")));
  ASSERT_TRUE(key_store.DeleteByPrefix(kDefaultUser, "prefix"));
  SecureBlob blob;
  EXPECT_FALSE(key_store.Read(kDefaultUser, "prefix_test", &blob));

  // Test with a single non-matching key.
  ASSERT_TRUE(key_store.Write(kDefaultUser, "_prefix_", SecureBlob("test")));
  ASSERT_TRUE(key_store.DeleteByPrefix(kDefaultUser, "prefix"));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "_prefix_", &blob));

  // Test with an empty prefix.
  ASSERT_TRUE(key_store.DeleteByPrefix(kDefaultUser, ""));
  EXPECT_FALSE(key_store.Read(kDefaultUser, "_prefix_", &blob));

  // Test with multiple matching and non-matching keys.
  const int kNumKeys = 110;  // Pkcs11KeyStore max is 100 for FindObjects.
  key_store.Write(kDefaultUser, "other1", SecureBlob("test"));
  for (int i = 0; i < kNumKeys; ++i) {
    string key_name = string("prefix") + base::IntToString(i);
    key_store.Write(kDefaultUser, key_name, SecureBlob(key_name));
  }
  ASSERT_TRUE(key_store.Write(kDefaultUser, "other2", SecureBlob("test")));
  ASSERT_TRUE(key_store.DeleteByPrefix(kDefaultUser, "prefix"));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "other1", &blob));
  EXPECT_TRUE(key_store.Read(kDefaultUser, "other2", &blob));
  for (int i = 0; i < kNumKeys; ++i) {
    string key_name = string("prefix") + base::IntToString(i);
    EXPECT_FALSE(key_store.Read(kDefaultUser, key_name, &blob));
  }
}

}  // namespace cryptohome
