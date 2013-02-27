// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pkcs11_keystore.h"

#include <map>
#include <string>
#include <vector>

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
    ON_CALL(pkcs11_, OpenSession(0, _, _))
        .WillByDefault(DoAll(SetArgumentPointee<2>(kDefaultHandle), Return(0)));
    ON_CALL(pkcs11_, CloseSession(kDefaultHandle))
        .WillByDefault(Return(0));
    ON_CALL(pkcs11_, CreateObject(kDefaultHandle, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::CreateObject));
    ON_CALL(pkcs11_, GetAttributeValue(kDefaultHandle, kDefaultHandle, _, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::GetAttributeValue));
    ON_CALL(pkcs11_, SetAttributeValue(kDefaultHandle, kDefaultHandle, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::SetAttributeValue));
    ON_CALL(pkcs11_, FindObjectsInit(kDefaultHandle, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::FindObjectsInit));
    ON_CALL(pkcs11_, FindObjects(kDefaultHandle, 1, _))
        .WillByDefault(Invoke(this, &KeyStoreTest::FindObjects));
    ON_CALL(pkcs11_, FindObjectsFinal(kDefaultHandle))
        .WillByDefault(Return(0));
  }

  // Stores a new labeled object, only CKA_LABEL and CKA_VALUE are relevant.
  virtual uint32_t CreateObject(uint64_t session_id,
                                const vector<uint8_t>& attributes,
                                uint64_t* new_object_handle) {
    *new_object_handle = kDefaultHandle;
    objects_[GetValue(attributes, CKA_LABEL)] = GetValue(attributes, CKA_VALUE);
    return CKR_OK;
  }

  // Supports reading CKA_VALUE.
  virtual uint32_t GetAttributeValue(uint64_t session_id,
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
      uint64_t session_id,
      uint64_t object_handle,
      const std::vector<uint8_t>& attributes) {
    objects_[current_object_] = GetValue(attributes, CKA_VALUE);
    return CKR_OK;
  }

  // Finds stored objects by CKA_LABEL.  Since we always use kDefaultHandle, the
  // only side-effect needs to be whether an object was found (find_status_).
  virtual uint32_t FindObjectsInit(uint64_t session_id,
                                   const vector<uint8_t>& attributes) {
    string label = GetValue(attributes, CKA_LABEL);
    find_status_ = (objects_.find(label) != objects_.end());
    if (find_status_)
      current_object_ = label;
    return CKR_OK;
  }

  // Reports a 'found' object based on find_status_.
  virtual uint32_t FindObjects(uint64_t session_id,
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
}

// Tests the key store when PKCS #11 fails to open a session.
TEST_F(KeyStoreTest, NoSession) {
  EXPECT_CALL(pkcs11_, OpenSession(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_FALSE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to create an object.
TEST_F(KeyStoreTest, CreateObjectFail) {
  EXPECT_CALL(pkcs11_, CreateObject(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_FALSE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to read attribute values.
TEST_F(KeyStoreTest, ReadValueFail) {
  EXPECT_CALL(pkcs11_, GetAttributeValue(_, _, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 fails to write attribute values.
TEST_F(KeyStoreTest, WriteValueFail) {
  EXPECT_CALL(pkcs11_, SetAttributeValue(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Write("test", SecureBlob("test_data2")));
}

// Tests the key store when PKCS #11 fails to find objects.  Tests each part of
// the multi-part find operation individually.
TEST_F(KeyStoreTest, FindFail) {
  EXPECT_CALL(pkcs11_, FindObjectsInit(_, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));

  EXPECT_CALL(pkcs11_, FindObjectsInit(_, _))
      .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(pkcs11_, FindObjects(_, _, _))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));

  EXPECT_CALL(pkcs11_, FindObjects(_, _, _))
      .WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(pkcs11_, FindObjectsFinal(_))
      .WillRepeatedly(Return(CKR_GENERAL_ERROR));
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

// Tests the key store when PKCS #11 successfully finds zero objects.
TEST_F(KeyStoreTest, FindNoObjects) {
  vector<uint64_t> empty;
  EXPECT_CALL(pkcs11_, FindObjects(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(empty), Return(CKR_OK)));
  Pkcs11KeyStore key_store;
  SecureBlob blob;
  EXPECT_TRUE(key_store.Write("test", SecureBlob("test_data")));
  EXPECT_FALSE(key_store.Read("test", &blob));
}

}  // namespace cryptohome
