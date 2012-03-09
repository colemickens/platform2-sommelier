// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The expensive PKCS #11 operations that occur during a VPN connect are C_Login
// and C_Sign.  This program replays these along with minimal overhead calls.
// The --generate switch can be used to prepare a private key to test against.

#include <stdio.h>
#include <stdlib.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/time.h>
#include <chromeos/syslog_logging.h>

#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

static const char* kKeyID = "test";

// Initializes the library and finds an appropriate slot.
static CK_SLOT_ID Initialize() {
  CK_RV result = C_Initialize(NULL);
  LOG(INFO) << "C_Initialize: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);

  CK_SLOT_ID slot_list[10];
  CK_ULONG slot_count = arraysize(slot_list);
  result = C_GetSlotList(CK_TRUE, slot_list, &slot_count);
  LOG(INFO) << "C_GetSlotList: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  if (slot_count == 0) {
    LOG(INFO) << "No slots.";
    exit(-1);
  }
  return slot_list[0];
}

// Opens a new session and performs a login. If force_login is set to true and
// the token is already logged in, it will be logged out and logged in again.
static CK_SESSION_HANDLE Login(CK_SLOT_ID slot, bool force_login) {
  CK_RV result = CKR_OK;
  CK_SESSION_HANDLE session = 0;
  bool try_again = true;
  while (try_again) {
    try_again = false;
    result = C_OpenSession(slot,
                           CKF_SERIAL_SESSION | CKF_RW_SESSION,
                           NULL, /* Ignore callbacks. */
                           NULL, /* Ignore callbacks. */
                           &session);
    LOG(INFO) << "C_OpenSession: " << chaps::CK_RVToString(result);
    if (result != CKR_OK)
      exit(-1);
    result = C_Login(session, CKU_USER, (CK_UTF8CHAR_PTR)"111111", 6);
    LOG(INFO) << "C_Login: " << chaps::CK_RVToString(result);
    if (result != CKR_OK && result != CKR_USER_ALREADY_LOGGED_IN)
      exit(-1);
    if (result == CKR_USER_ALREADY_LOGGED_IN && force_login) {
      try_again = true;
      result = C_Logout(session);
      LOG(INFO) << "C_Logout: " << chaps::CK_RVToString(result);
      if (result != CKR_OK)
        exit(-1);
      result = C_CloseAllSessions(slot);
      LOG(INFO) << "C_CloseAllSessions: " << chaps::CK_RVToString(result);
      if (result != CKR_OK)
        exit(-1);
    }
  }
  return session;
}

// Sign some data with a private key.
static void Sign(CK_SESSION_HANDLE session) {
  CK_OBJECT_CLASS class_value = CKO_PRIVATE_KEY;
  CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, &class_value, sizeof(class_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)}
  };
  CK_RV result = C_FindObjectsInit(session, attributes, arraysize(attributes));
  LOG(INFO) << "C_FindObjectsInit: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);

  CK_OBJECT_HANDLE object = 0;
  CK_ULONG object_count = 1;
  result = C_FindObjects(session, &object, 1, &object_count);
  LOG(INFO) << "C_FindObjects: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  result = C_FindObjectsFinal(session);
  LOG(INFO) << "C_FindObjectsFinal: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  if (object_count == 0) {
    LOG(INFO) << "No key.";
    exit(-1);
  }

  CK_MECHANISM mechanism;
  mechanism.mechanism = CKM_SHA1_RSA_PKCS;
  mechanism.pParameter = NULL;
  mechanism.ulParameterLen = 0;
  result = C_SignInit(session, &mechanism, object);
  LOG(INFO) << "C_SignInit: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);

  CK_BYTE data[200] = {0};
  CK_BYTE signature[256] = {0};
  CK_ULONG signature_length = arraysize(signature);
  result = C_Sign(session,
                  data, arraysize(data),
                  signature, &signature_length);
  LOG(INFO) << "C_Sign: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
}

// Generates a test key pair.
static void GenerateKeyPair(CK_SESSION_HANDLE session, bool is_temp) {
  CK_MECHANISM mechanism;
  mechanism.mechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
  mechanism.pParameter = NULL;
  mechanism.ulParameterLen = 0;
  CK_ULONG bits = 2048;
  CK_BYTE e[] = {1, 0, 1};
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_ATTRIBUTE public_attributes[] = {
    {CKA_ENCRYPT, &true_value, sizeof(true_value)},
    {CKA_VERIFY, &true_value, sizeof(true_value)},
    {CKA_WRAP, &false_value, sizeof(false_value)},
    {CKA_TOKEN, &true_value, sizeof(true_value)},
    {CKA_PRIVATE, &false_value, sizeof(false_value)},
    {CKA_MODULUS_BITS, &bits, sizeof(bits)},
    {CKA_PUBLIC_EXPONENT, e, sizeof(e)}
  };
  CK_ATTRIBUTE private_attributes[] = {
    {CKA_DECRYPT, &true_value, sizeof(true_value)},
    {CKA_SIGN, &true_value, sizeof(true_value)},
    {CKA_UNWRAP, &false_value, sizeof(false_value)},
    {CKA_SENSITIVE, &true_value, sizeof(true_value)},
    {CKA_TOKEN, &true_value, sizeof(true_value)},
    {CKA_PRIVATE, &true_value, sizeof(true_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)},
  };
  CK_OBJECT_HANDLE public_key_handle = 0;
  CK_OBJECT_HANDLE private_key_handle = 0;
  CK_RV result = C_GenerateKeyPair(session,
                                   &mechanism,
                                   public_attributes,
                                   arraysize(public_attributes),
                                   private_attributes,
                                   arraysize(private_attributes),
                                   &public_key_handle,
                                   &private_key_handle);
  LOG(INFO) << "C_GenerateKeyPair: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  if (is_temp) {
    result = C_DestroyObject(session, public_key_handle);
    LOG(INFO) << "C_DestroyObject: " << chaps::CK_RVToString(result);
    result = C_DestroyObject(session, private_key_handle);
    LOG(INFO) << "C_DestroyObject: " << chaps::CK_RVToString(result);
  }
}

// Cleans up the session and library.
static void TearDown(CK_SESSION_HANDLE session) {
  CK_RV result = C_Logout(session);
  LOG(INFO) << "C_Logout: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);

  result = C_CloseSession(session);
  LOG(INFO) << "C_CloseSession: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);

  C_Finalize(NULL);
}

void PrintHelp() {
  printf("Usage: p11_replay [COMMAND]\n");
  printf("Commands:\n");
  printf("  --generate : Generates a key pair suitable for replay tests.\n");
  printf("  --generate_delete : Generates a key pair and deletes it. This is "
         "useful for comparing key generation on different TPM models\n");
  printf("  --replay_vpn : Replays a L2TP/IPSEC VPN negotiation.\n");
  printf("  --replay_wifi : Replays a EAP-TLS Wifi negotiation. This is the "
         "default command if no command is specified.\n");
}

void PrintTicks(base::TimeTicks* start_ticks) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - *start_ticks;
  *start_ticks = now;
  long long int value = delta.InMillisecondsRoundedUp();
  printf("Elapsed: %lldms\n", value);
}

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("h") || cl->HasSwitch("help")) {
    PrintHelp();
    return 0;
  }
  bool generate = cl->HasSwitch("generate");
  bool generate_delete = cl->HasSwitch("generate_delete");
  bool vpn = cl->HasSwitch("replay_vpn");
  chromeos::InitLog(chromeos::kLogToStderr);

  base::TimeTicks start_ticks = base::TimeTicks::Now();
  CK_SLOT_ID slot = Initialize();
  PrintTicks(&start_ticks);
  CK_SESSION_HANDLE session;
  if (generate || generate_delete) {
    session = Login(slot, false);
    PrintTicks(&start_ticks);
    GenerateKeyPair(session, generate_delete);
    PrintTicks(&start_ticks);
  } else {
    printf("Replay 1 of 2\n");
    session = Login(slot, vpn);
    Sign(session);
    PrintTicks(&start_ticks);
    printf("Replay 2 of 2\n");
    CK_SESSION_HANDLE session2 = Login(slot, vpn);
    Sign(session2);
    PrintTicks(&start_ticks);
    C_CloseSession(session2);
  }
  TearDown(session);
  PrintTicks(&start_ticks);
}
