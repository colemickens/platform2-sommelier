// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "session_impl.h"

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

#include "chaps_factory_mock.h"
#include "object_pool_mock.h"
#include "object_mock.h"
#include "tpm_utility_mock.h"

using std::string;
using std::vector;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

static const string kAuthData("000000");
static const string kNewAuthData("111111");

static void ConfigureObjectPool(ObjectPoolMock* op) {
  op->SetupFake();
  EXPECT_CALL(*op, Insert(_)).Times(AnyNumber());
  EXPECT_CALL(*op, Find(_, _)).Times(AnyNumber());
  EXPECT_CALL(*op, Delete(_)).Times(AnyNumber());
}

static ObjectPool* CreateObjectPoolMock() {
  ObjectPoolMock* op = new ObjectPoolMock();
  ConfigureObjectPool(op);
  return op;
}

static Object* CreateObjectMock() {
  ObjectMock* o = new ObjectMock();
  o->SetupFake();
  EXPECT_CALL(*o, GetObjectClass()).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributes(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, FinalizeNewObject()).WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(*o, Copy(_)).WillRepeatedly(Return(CKR_OK));
  EXPECT_CALL(*o, IsTokenObject()).Times(AnyNumber());
  EXPECT_CALL(*o, IsAttributePresent(_)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeString(_)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeInt(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, GetAttributeBool(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributeString(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributeInt(_, _)).Times(AnyNumber());
  EXPECT_CALL(*o, SetAttributeBool(_, _)).Times(AnyNumber());
  return o;
}

static bool FakeRandom(int num_bytes, string* random) {
  *random = string(num_bytes, 0);
  return true;
}

static void ConfigureTPMUtility(TPMUtilityMock* tpm) {
  EXPECT_CALL(*tpm, GenerateRandom(_, _)).WillRepeatedly(Invoke(FakeRandom));
}

// Test fixture for an initialized SessionImpl instance.
class TestSession: public ::testing::Test {
 public:
  TestSession() {
    EXPECT_CALL(factory_, CreateObject())
        .WillRepeatedly(InvokeWithoutArgs(CreateObjectMock));
    EXPECT_CALL(factory_, CreateObjectPool())
        .WillRepeatedly(InvokeWithoutArgs(CreateObjectPoolMock));
    ConfigureObjectPool(&token_pool_);
    ConfigureTPMUtility(&tpm_);
  }
  void SetUp() {
    session_.reset(new SessionImpl(1, &token_pool_, &tpm_, &factory_, false));
  }
  void GenerateSecretKey(CK_MECHANISM_TYPE mechanism, int size, Object** obj) {
    CK_BBOOL no = CK_FALSE;
    CK_BBOOL yes = CK_TRUE;
    CK_ATTRIBUTE encdec_template[] = {
      {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
      {CKA_ENCRYPT, &yes, sizeof(CK_BBOOL)},
      {CKA_DECRYPT, &yes, sizeof(CK_BBOOL)},
      {CKA_VALUE_LEN, &size, sizeof(int)}
    };
    CK_ATTRIBUTE signverify_template[] = {
      {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
      {CKA_SIGN, &yes, sizeof(CK_BBOOL)},
      {CKA_VERIFY, &yes, sizeof(CK_BBOOL)},
      {CKA_VALUE_LEN, &size, sizeof(int)}
    };
    CK_ATTRIBUTE_PTR attr = encdec_template;
    if (mechanism == CKM_GENERIC_SECRET_KEY_GEN)
      attr = signverify_template;
    int handle = 0;
    ASSERT_EQ(CKR_OK, session_->GenerateKey(mechanism, "", attr, 4, &handle));
    ASSERT_TRUE(session_->GetObject(handle, obj));
  }
  void GenerateRSAKeyPair(bool signing, int size,
                          Object** pub, Object** priv) {
    CK_BBOOL no = CK_FALSE;
    CK_BBOOL yes = CK_TRUE;
    CK_BYTE pubexp[] = {1, 0, 1};
    CK_ATTRIBUTE pub_attr[] = {
      {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
      {CKA_ENCRYPT, signing ? &no : &yes, sizeof(CK_BBOOL)},
      {CKA_VERIFY, signing ? &yes : &no, sizeof(CK_BBOOL)},
      {CKA_PUBLIC_EXPONENT, pubexp, 3},
      {CKA_MODULUS_BITS, &size, sizeof(int)}
    };
    CK_ATTRIBUTE priv_attr[] = {
      {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
      {CKA_DECRYPT, signing ? &no : &yes, sizeof(CK_BBOOL)},
      {CKA_SIGN, signing ? &yes : &no, sizeof(CK_BBOOL)}
    };
    int pubh = 0, privh = 0;
    ASSERT_EQ(CKR_OK, session_->GenerateKeyPair(CKM_RSA_PKCS_KEY_PAIR_GEN, "",
                                                pub_attr, 5, priv_attr, 3,
                                                &pubh, &privh));
    ASSERT_TRUE(session_->GetObject(pubh, pub));
    ASSERT_TRUE(session_->GetObject(privh, priv));
  }
  int CreateObject() {
    CK_OBJECT_CLASS c = CKO_DATA;
    CK_BBOOL no = CK_FALSE;
    CK_ATTRIBUTE attr[] = {
      {CKA_CLASS, &c, sizeof(CK_OBJECT_CLASS)},
      {CKA_TOKEN, &no, sizeof(CK_BBOOL)}
    };
    int h;
    session_->CreateObject(attr, 2, &h);
    return h;
  }
 protected:
  ObjectPoolMock token_pool_;
  ChapsFactoryMock factory_;
  TPMUtilityMock tpm_;
  scoped_ptr<SessionImpl> session_;
};

typedef TestSession TestSession_DeathTest;

// Test that SessionImpl asserts as expected when not properly initialized.
TEST(DeathTest, InvalidInit) {
  ObjectPoolMock pool;
  ChapsFactoryMock factory;
  TPMUtilityMock tpm;
  SessionImpl* session;
  EXPECT_DEATH_IF_SUPPORTED(
      session = new SessionImpl(1, NULL, &tpm, &factory, false),
      "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      session = new SessionImpl(1, &pool, NULL, &factory, false),
      "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      session = new SessionImpl(1, &pool, &tpm, NULL, false),
      "Check failed");
  (void)session;
}

// Test that SessionImpl asserts as expected when passed invalid arguments.
TEST_F(TestSession_DeathTest, InvalidArgs) {
  OperationType invalid_op = kNumOperationTypes;
  EXPECT_DEATH_IF_SUPPORTED(session_->IsOperationActive(invalid_op),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->CreateObject(NULL, 0, NULL),
                            "Check failed");
  int i;
  EXPECT_DEATH_IF_SUPPORTED(session_->CreateObject(NULL, 1, &i),
                            "Check failed");
  i = CreateObject();
  EXPECT_DEATH_IF_SUPPORTED(session_->CopyObject(NULL, 0, i, NULL),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->CopyObject(NULL, 1, i, &i),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->FindObjects(invalid_op, NULL),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->GetObject(1, NULL),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationInit(invalid_op, 0, "", NULL),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationInit(kEncrypt, CKM_AES_CBC, "",
                                                    NULL),
                            "Check failed");
  string s;
  Object* o;
  GenerateSecretKey(CKM_AES_KEY_GEN, 32, &o);
  ASSERT_EQ(CKR_OK, session_->OperationInit(kEncrypt, CKM_AES_ECB, "", o));
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationUpdate(invalid_op, "", &i, &s),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationUpdate(kEncrypt, "", NULL, &s),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationUpdate(kEncrypt, "", &i, NULL),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationFinal(invalid_op, &i, &s),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationFinal(kEncrypt, NULL, &s),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->OperationFinal(kEncrypt, &i, NULL),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(
      session_->OperationSinglePart(invalid_op, "", &i, &s),
      "Check failed");
}

// Test that SessionImpl asserts when out-of-memory during initialization.
TEST(DeathTest, OutOfMemoryInit) {
  ObjectPoolMock pool;
  TPMUtilityMock tpm;
  ChapsFactoryMock factory;
  ObjectPool* null_pool = NULL;
  EXPECT_CALL(factory, CreateObjectPool())
      .WillRepeatedly(Return(null_pool));
  Session* session;
  EXPECT_DEATH_IF_SUPPORTED(
      session = new SessionImpl(1, &pool, &tpm, &factory, false),
      "Check failed");
  (void)session;
}

// Test that SessionImpl asserts when out-of-memory during object creation.
TEST_F(TestSession_DeathTest, OutOfMemoryObject) {
  Object* null_object = NULL;
  EXPECT_CALL(factory_, CreateObject())
      .WillRepeatedly(Return(null_object));
  int tmp;
  EXPECT_DEATH_IF_SUPPORTED(session_->CreateObject(NULL, 0, &tmp),
                            "Check failed");
  EXPECT_DEATH_IF_SUPPORTED(session_->FindObjectsInit(NULL, 0), "Check failed");
}

// Test that default session properties are correctly reported.
TEST_F(TestSession, DefaultSetup) {
  EXPECT_EQ(1, session_->GetSlot());
  EXPECT_FALSE(session_->IsReadOnly());
  EXPECT_FALSE(session_->IsOperationActive(kEncrypt));
}

// Test object management: create / copy / find / destroy.
TEST_F(TestSession, Objects) {
  EXPECT_CALL(token_pool_, Insert(_)).Times(2);
  EXPECT_CALL(token_pool_, Find(_, _)).Times(1);
  EXPECT_CALL(token_pool_, Delete(_)).Times(1);
  CK_OBJECT_CLASS oc = CKO_SECRET_KEY;
  CK_ATTRIBUTE attr[] = {{CKA_CLASS, &oc, sizeof(oc)}};
  int handle = 0;
  int invalid_handle = -1;
  // Create a new object.
  ASSERT_EQ(CKR_OK, session_->CreateObject(attr, 1, &handle));
  EXPECT_GT(handle, 0);
  Object* o;
  // Get the new object from the new handle.
  EXPECT_TRUE(session_->GetObject(handle, &o));
  int handle2 = 0;
  // Copy an object (try invalid and valid handles).
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID,
            session_->CopyObject(attr, 1, invalid_handle, &handle2));
  ASSERT_EQ(CKR_OK, session_->CopyObject(attr, 1, handle, &handle2));
  // Ensure handles are unique.
  EXPECT_TRUE(handle != handle2);
  EXPECT_TRUE(session_->GetObject(handle2, &o));
  EXPECT_FALSE(session_->GetObject(invalid_handle, &o));
  vector<int> v;
  // Find objects with calls out-of-order.
  EXPECT_EQ(CKR_OPERATION_NOT_INITIALIZED, session_->FindObjects(1, &v));
  EXPECT_EQ(CKR_OPERATION_NOT_INITIALIZED, session_->FindObjectsFinal());
  // Find the objects we've created (there should be 2).
  EXPECT_EQ(CKR_OK, session_->FindObjectsInit(attr, 1));
  EXPECT_EQ(CKR_OPERATION_ACTIVE, session_->FindObjectsInit(attr, 1));
  // Test multi-step finds by only allowing 1 result at a time.
  EXPECT_EQ(CKR_OK, session_->FindObjects(1, &v));
  EXPECT_EQ(1, v.size());
  EXPECT_EQ(CKR_OK, session_->FindObjects(1, &v));
  EXPECT_EQ(2, v.size());
  // We have them all but we'll query again to make sure it behaves properly.
  EXPECT_EQ(CKR_OK, session_->FindObjects(1, &v));
  ASSERT_EQ(2, v.size());
  // Check that the handles found are the same ones we know about.
  EXPECT_TRUE(v[0] == handle || v[1] == handle);
  EXPECT_TRUE(v[0] == handle2 || v[1] == handle2);
  EXPECT_EQ(CKR_OK, session_->FindObjectsFinal());
  // Destroy an object (try invalid and valid handles).
  EXPECT_EQ(CKR_OBJECT_HANDLE_INVALID, session_->DestroyObject(invalid_handle));
  EXPECT_EQ(CKR_OK, session_->DestroyObject(handle));
  // Once destroyed, we should not be able to use the handle.
  EXPECT_FALSE(session_->GetObject(handle, &o));
}

// Test multi-part and single-part cipher operations.
TEST_F(TestSession, Cipher) {
  Object* key_object = NULL;
  GenerateSecretKey(CKM_AES_KEY_GEN, 32, &key_object);
  EXPECT_EQ(CKR_OK, session_->OperationInit(kEncrypt,
                                            CKM_AES_CBC_PAD,
                                            string(16, 'A'),
                                            key_object));
  string in(22, 'B');
  string out, tmp;
  int maxlen = 0;
  // Check buffer-too-small semantics (and for each call following).
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationUpdate(kEncrypt, in, &maxlen, &tmp));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kEncrypt, in, &maxlen, &tmp));
  out += tmp;
  // The first block is ready, check that we've received it.
  EXPECT_EQ(16, out.length());
  maxlen = 0;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationFinal(kEncrypt, &maxlen, &tmp));
  EXPECT_EQ(CKR_OK, session_->OperationFinal(kEncrypt, &maxlen, &tmp));
  out += tmp;
  // Check that we've received the final block.
  EXPECT_EQ(32, out.length());
  EXPECT_EQ(CKR_OK, session_->OperationInit(kDecrypt,
                                            CKM_AES_CBC_PAD,
                                            string(16, 'A'),
                                            key_object));
  string in2;
  maxlen = 0;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationSinglePart(kDecrypt, out, &maxlen, &in2));
  EXPECT_EQ(CKR_OK,
            session_->OperationSinglePart(kDecrypt, out, &maxlen, &in2));
  EXPECT_EQ(22, in2.length());
  // Check that what has been decrypted matches our original plain-text.
  EXPECT_TRUE(in == in2);
}

// Test multi-part and single-part digest operations.
TEST_F(TestSession, Digest) {
  string in(30, 'A');
  EXPECT_EQ(CKR_OK, session_->OperationInit(kDigest, CKM_SHA_1, "", NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kDigest,
                                              in.substr(0, 10),
                                              NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kDigest,
                                              in.substr(10, 10),
                                              NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kDigest,
                                              in.substr(20, 10),
                                              NULL,
                                              NULL));
  int len = 0;
  string out;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationFinal(kDigest, &len, &out));
  EXPECT_EQ(20, len);
  EXPECT_EQ(CKR_OK, session_->OperationFinal(kDigest, &len, &out));
  EXPECT_EQ(CKR_OK, session_->OperationInit(kDigest, CKM_SHA_1, "", NULL));
  string out2;
  len = 0;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationSinglePart(kDigest, in, &len, &out2));
  EXPECT_EQ(CKR_OK, session_->OperationSinglePart(kDigest, in, &len, &out2));
  EXPECT_EQ(20, len);
  // Check that both operations computed the same digest.
  EXPECT_TRUE(out == out2);
}

// Test HMAC sign and verify operations.
TEST_F(TestSession, HMAC) {
  Object* key_object = NULL;
  GenerateSecretKey(CKM_GENERIC_SECRET_KEY_GEN, 32, &key_object);
  string in(30, 'A');
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kSign, CKM_SHA256_HMAC, "", key_object));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kSign,
                                              in.substr(0, 10),
                                              NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kSign,
                                              in.substr(10, 10),
                                              NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kSign,
                                              in.substr(20, 10),
                                              NULL,
                                              NULL));
  int len = 0;
  string out;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL, session_->OperationFinal(kSign, &len, &out));
  EXPECT_EQ(CKR_OK, session_->OperationFinal(kSign, &len, &out));
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kVerify, CKM_SHA256_HMAC, "", key_object));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, in, NULL, NULL));
  // A successful verify implies both operations computed the same MAC.
  EXPECT_EQ(CKR_OK, session_->VerifyFinal(out));
}

// Test RSA PKCS #1 encryption.
TEST_F(TestSession, RSAEncrypt) {
  Object* pub = NULL;
  Object* priv = NULL;
  GenerateRSAKeyPair(false, 1024, &pub, &priv);
  EXPECT_EQ(CKR_OK, session_->OperationInit(kEncrypt, CKM_RSA_PKCS, "", pub));
  string in(100, 'A');
  int len = 0;
  string out;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationSinglePart(kEncrypt, in, &len, &out));
  EXPECT_EQ(CKR_OK, session_->OperationSinglePart(kEncrypt, in, &len, &out));

  EXPECT_EQ(CKR_OK, session_->OperationInit(kDecrypt, CKM_RSA_PKCS, "", priv));
  len = 0;
  string in2 = out;
  string out2;
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kDecrypt, in2, &len, &out2));
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationFinal(kDecrypt, &len, &out2));
  EXPECT_EQ(CKR_OK, session_->OperationFinal(kDecrypt, &len, &out2));
  EXPECT_EQ(in.length(), out2.length());
  // Check that what has been decrypted matches our original plain-text.
  EXPECT_TRUE(in == out2);
}

// Test RSA PKCS #1 sign / verify.
TEST_F(TestSession, RSASign) {
  Object* pub = NULL;
  Object* priv = NULL;
  GenerateRSAKeyPair(true, 1024, &pub, &priv);
  // Sign / verify without a built-in hash.
  EXPECT_EQ(CKR_OK, session_->OperationInit(kSign, CKM_RSA_PKCS, "", priv));
  string in(100, 'A');
  int len = 0;
  string sig;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL,
            session_->OperationSinglePart(kSign, in, &len, &sig));
  EXPECT_EQ(CKR_OK, session_->OperationSinglePart(kSign, in, &len, &sig));
  EXPECT_EQ(CKR_OK, session_->OperationInit(kVerify, CKM_RSA_PKCS, "", pub));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, in, NULL, NULL));
  EXPECT_EQ(CKR_OK, session_->VerifyFinal(sig));
  // Sign / verify with a built-in SHA-256 hash.
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kSign, CKM_SHA256_RSA_PKCS, "", priv));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kSign, in.substr(0, 50), NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kSign, in.substr(50, 50), NULL,
                                              NULL));
  string sig2;
  len = 0;
  EXPECT_EQ(CKR_BUFFER_TOO_SMALL, session_->OperationFinal(kSign, &len, &sig2));
  EXPECT_EQ(CKR_OK, session_->OperationFinal(kSign, &len, &sig2));
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kVerify, CKM_SHA256_RSA_PKCS, "", pub));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, in.substr(0, 20), NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, in.substr(20, 80), NULL,
                                              NULL));
  EXPECT_EQ(CKR_OK, session_->VerifyFinal(sig2));
}

// Test that requests for unsupported mechanisms are handled correctly.
TEST_F(TestSession, MechanismInvalid) {
  Object* key = NULL;
  // Use a valid key so that key errors don't mask mechanism errors.
  GenerateSecretKey(CKM_AES_KEY_GEN, 16, &key);
  // We don't support IDEA.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kEncrypt, CKM_IDEA_CBC, "", key));
  // We don't support SHA-224.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kSign, CKM_SHA224_RSA_PKCS, "", key));
  // We don't support MD2.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kDigest, CKM_MD2, "", NULL));
}

// Test that operation / mechanism mismatches are handled correctly.
TEST_F(TestSession, MechanismMismatch) {
  Object* hmac = NULL;
  GenerateSecretKey(CKM_GENERIC_SECRET_KEY_GEN, 16, &hmac);
  Object* aes = NULL;
  GenerateSecretKey(CKM_AES_KEY_GEN, 16, &aes);
  // Encrypt with a sign/verify mechanism.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kEncrypt, CKM_SHA_1_HMAC, "", hmac));
  // Sign with an encryption mechanism.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kSign, CKM_AES_CBC_PAD, "", aes));
  // Sign with a digest-only mechanism.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kSign, CKM_SHA_1, "", hmac));
  // Digest with a sign+digest mechanism.
  EXPECT_EQ(CKR_MECHANISM_INVALID,
           session_->OperationInit(kDigest, CKM_SHA1_RSA_PKCS, "", NULL));
}

// Test that mechanism / key type mismatches are handled correctly.
TEST_F(TestSession, KeyTypeMismatch) {
  Object* aes = NULL;
  GenerateSecretKey(CKM_AES_KEY_GEN, 16, &aes);
  Object* rsapub = NULL;
  Object* rsapriv = NULL;
  GenerateRSAKeyPair(true, 512, &rsapub, &rsapriv);
  // DES3 with an AES key.
  EXPECT_EQ(CKR_KEY_TYPE_INCONSISTENT,
           session_->OperationInit(kEncrypt, CKM_DES3_CBC, "", aes));
  // AES with an RSA key.
  EXPECT_EQ(CKR_KEY_TYPE_INCONSISTENT,
           session_->OperationInit(kEncrypt, CKM_AES_CBC, "", rsapriv));
  // HMAC with an RSA key.
  EXPECT_EQ(CKR_KEY_TYPE_INCONSISTENT,
           session_->OperationInit(kSign, CKM_SHA_1_HMAC, "", rsapriv));
  // RSA with an AES key.
  EXPECT_EQ(CKR_KEY_TYPE_INCONSISTENT,
           session_->OperationInit(kSign, CKM_SHA1_RSA_PKCS, "", aes));
}

// Test that key function permissions are correctly enforced.
TEST_F(TestSession, KeyFunctionPermission) {
  Object* encpub = NULL;
  Object* encpriv = NULL;
  GenerateRSAKeyPair(false, 512, &encpub, &encpriv);
  Object* sigpub = NULL;
  Object* sigpriv = NULL;
  GenerateRSAKeyPair(true, 512, &sigpub, &sigpriv);
  // Try decrypting with a sign-only key.
  EXPECT_EQ(CKR_KEY_FUNCTION_NOT_PERMITTED,
           session_->OperationInit(kDecrypt, CKM_RSA_PKCS, "", sigpriv));
  // Try signing with a decrypt-only key.
  EXPECT_EQ(CKR_KEY_FUNCTION_NOT_PERMITTED,
           session_->OperationInit(kSign, CKM_RSA_PKCS, "", encpriv));
}

// Test that invalid mechanism parameters for ciphers are handled correctly.
TEST_F(TestSession, BadIV) {
  Object* aes = NULL;
  GenerateSecretKey(CKM_AES_KEY_GEN, 16, &aes);
  Object* des = NULL;
  GenerateSecretKey(CKM_DES_KEY_GEN, 16, &des);
  Object* des3 = NULL;
  GenerateSecretKey(CKM_DES3_KEY_GEN, 16, &des3);
  // AES expects 16 bytes and DES/DES3 expects 8 bytes.
  string bad_iv(7, 0);
  EXPECT_EQ(CKR_MECHANISM_PARAM_INVALID,
            session_->OperationInit(kEncrypt, CKM_AES_CBC, bad_iv, aes));
  EXPECT_EQ(CKR_MECHANISM_PARAM_INVALID,
            session_->OperationInit(kEncrypt, CKM_DES_CBC, bad_iv, des));
  EXPECT_EQ(CKR_MECHANISM_PARAM_INVALID,
            session_->OperationInit(kEncrypt, CKM_DES3_CBC, bad_iv, des3));

}

// Test that invalid key size ranges are handled correctly.
TEST_F(TestSession, BadKeySize) {
  Object* key = NULL;
  GenerateSecretKey(CKM_AES_KEY_GEN, 16, &key);
  // AES keys can be 16, 24, or 32 bytes in length.
  key->SetAttributeString(CKA_VALUE, string(33, 0));
  EXPECT_EQ(CKR_KEY_SIZE_RANGE,
            session_->OperationInit(kEncrypt, CKM_AES_ECB, "", key));
  Object* pub = NULL;
  Object* priv = NULL;
  GenerateRSAKeyPair(true, 512, &pub, &priv);
  // RSA keys can have a modulus size no smaller than 512.
  priv->SetAttributeString(CKA_MODULUS, string(300, 0));
  EXPECT_EQ(CKR_KEY_SIZE_RANGE,
            session_->OperationInit(kSign, CKM_RSA_PKCS, "", priv));
}

// Test that invalid attributes for key pair generation are handled correctly.
TEST_F(TestSession, BadRSAGenerate) {
  CK_BBOOL no = CK_FALSE;
  int size = 1024;
  CK_BYTE pubexp[] = {1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  CK_ATTRIBUTE pub_attr[] = {
    {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
    {CKA_PUBLIC_EXPONENT, pubexp, 12},
    {CKA_MODULUS_BITS, &size, sizeof(int)}
  };
  CK_ATTRIBUTE priv_attr[] = {
    {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
  };
  int pub, priv;
  // CKA_PUBLIC_EXPONENT too large.
  EXPECT_EQ(CKR_FUNCTION_FAILED,
            session_->GenerateKeyPair(CKM_RSA_PKCS_KEY_PAIR_GEN, "",
                                      pub_attr, 3,
                                      priv_attr, 1,
                                      &pub, &priv));
  pub_attr[1].ulValueLen = 3;
  size = 10000;
  // CKA_MODULUS_BITS too large.
  EXPECT_EQ(CKR_KEY_SIZE_RANGE,
            session_->GenerateKeyPair(CKM_RSA_PKCS_KEY_PAIR_GEN, "",
                                      pub_attr, 3,
                                      priv_attr, 1,
                                      &pub, &priv));
  // CKA_MODULUS_BITS missing.
  EXPECT_EQ(CKR_TEMPLATE_INCOMPLETE,
            session_->GenerateKeyPair(CKM_RSA_PKCS_KEY_PAIR_GEN, "",
                                      pub_attr, 2,
                                      priv_attr, 1,
                                      &pub, &priv));
}

// Test that invalid attributes for key generation are handled correctly.
TEST_F(TestSession, BadAESGenerate) {
  CK_BBOOL no = CK_FALSE;
  CK_BBOOL yes = CK_TRUE;
  int size = 33;
  CK_ATTRIBUTE attr[] = {
    {CKA_TOKEN, &no, sizeof(CK_BBOOL)},
    {CKA_ENCRYPT, &yes, sizeof(CK_BBOOL)},
    {CKA_DECRYPT, &yes, sizeof(CK_BBOOL)},
    {CKA_VALUE_LEN, &size, sizeof(int)}
  };
  int handle = 0;
  // CKA_VALUE_LEN missing.
  EXPECT_EQ(CKR_TEMPLATE_INCOMPLETE,
            session_->GenerateKey(CKM_AES_KEY_GEN, "", attr, 3, &handle));
  // CKA_VALUE_LEN out of range.
  EXPECT_EQ(CKR_KEY_SIZE_RANGE,
            session_->GenerateKey(CKM_AES_KEY_GEN, "", attr, 4, &handle));
}

// Test that signature verification fails as expected for invalid signatures.
TEST_F(TestSession, BadSignature) {
  string input(100, 'A');
  string signature(20, 0);
  Object* hmac;
  GenerateSecretKey(CKM_GENERIC_SECRET_KEY_GEN, 32, &hmac);
  Object* rsapub, *rsapriv;
  GenerateRSAKeyPair(true, 1024, &rsapub, &rsapriv);
  // HMAC with bad signature length.
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kVerify, CKM_SHA256_HMAC, "", hmac));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, input, NULL, NULL));
  EXPECT_EQ(CKR_SIGNATURE_LEN_RANGE,
            session_->VerifyFinal(signature));
  // HMAC with bad signature.
  signature.resize(32);
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kVerify, CKM_SHA256_HMAC, "", hmac));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, input, NULL, NULL));
  EXPECT_EQ(CKR_SIGNATURE_INVALID,
            session_->VerifyFinal(signature));
  // RSA with bad signature length.
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kVerify, CKM_RSA_PKCS, "", rsapub));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, input, NULL, NULL));
  EXPECT_EQ(CKR_SIGNATURE_LEN_RANGE,
            session_->VerifyFinal(signature));
  // RSA with bad signature.
  signature.resize(128, 1);
  EXPECT_EQ(CKR_OK,
            session_->OperationInit(kVerify, CKM_RSA_PKCS, "", rsapub));
  EXPECT_EQ(CKR_OK, session_->OperationUpdate(kVerify, input, NULL, NULL));
  EXPECT_EQ(CKR_SIGNATURE_INVALID,
            session_->VerifyFinal(signature));
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  OpenSSL_add_all_algorithms();
  ERR_load_crypto_strings();
  return RUN_ALL_TESTS();
}
