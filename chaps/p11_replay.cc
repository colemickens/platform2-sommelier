// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The expensive PKCS #11 operations that occur during a VPN connect are C_Login
// and C_Sign.  This program replays these along with minimal overhead calls.
// The --generate switch can be used to prepare a private key to test against.

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/time.h>
#include <chromeos/syslog_logging.h>
#include <openssl/rsa.h>

#include "chaps/chaps_utility.h"
#include "pkcs11/cryptoki.h"

using chaps::ConvertStringToByteBuffer;
using std::string;
using std::vector;

namespace {
const char* kKeyID = "test";

// Initializes the library and finds an appropriate slot.
CK_SLOT_ID Initialize() {
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
  LOG(INFO) << "Choosing slot " << slot_list[0];
  return slot_list[0];
}

// Opens a session on the given slot.
CK_SESSION_HANDLE OpenSession(CK_SLOT_ID slot) {
  CK_SESSION_HANDLE session = CK_INVALID_HANDLE;
  CK_RV result = C_OpenSession(slot,
                               CKF_SERIAL_SESSION | CKF_RW_SESSION,
                               NULL,  // Ignore callbacks.
                               NULL,  // Ignore callbacks.
                               &session);
  LOG(INFO) << "C_OpenSession: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  return session;
}

// Opens a new session and performs a login. If force_login is set to true and
// the token is already logged in, it will be logged out and logged in again. In
// this case, the session will also be closed and reopened. In any case, the
// current, valid session is returned.
CK_SESSION_HANDLE Login(CK_SLOT_ID slot,
                        bool force_login,
                        CK_SESSION_HANDLE session) {
  CK_RV result = CKR_OK;
  bool try_again = true;
  while (try_again) {
    try_again = false;
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
      session = OpenSession(slot);
    }
  }
  return session;
}

// Finds all objects matching the given attributes.
void Find(CK_SESSION_HANDLE session,
          CK_ATTRIBUTE attributes[],
          CK_ULONG num_attributes,
          vector<CK_OBJECT_HANDLE>* objects) {
  CK_RV result = C_FindObjectsInit(session, attributes, num_attributes);
  LOG(INFO) << "C_FindObjectsInit: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  CK_OBJECT_HANDLE object = 0;
  CK_ULONG object_count = 1;
  while (object_count > 0) {
    result = C_FindObjects(session, &object, 1, &object_count);
    LOG(INFO) << "C_FindObjects: " << chaps::CK_RVToString(result);
    if (result != CKR_OK)
      exit(-1);
    if (object_count > 0) {
      objects->push_back(object);
    }
  }
  result = C_FindObjectsFinal(session);
  LOG(INFO) << "C_FindObjectsFinal: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
}

// Sign some data with a private key.
void Sign(CK_SESSION_HANDLE session, const string& label) {
  CK_OBJECT_CLASS class_value = CKO_PRIVATE_KEY;
  CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, &class_value, sizeof(class_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)},
    {CKA_LABEL, const_cast<char*>(label.c_str()), label.length()},
  };
  vector<CK_OBJECT_HANDLE> objects;
  Find(session, attributes, arraysize(attributes), &objects);
  if (objects.size() == 0) {
    LOG(INFO) << "No key.";
    exit(-1);
  }

  CK_MECHANISM mechanism;
  mechanism.mechanism = CKM_SHA1_RSA_PKCS;
  mechanism.pParameter = NULL;
  mechanism.ulParameterLen = 0;
  CK_RV result = C_SignInit(session, &mechanism, objects[0]);
  LOG(INFO) << "C_SignInit: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);

  CK_BYTE data[200] = {0};
  CK_BYTE signature[2048] = {0};
  CK_ULONG signature_length = arraysize(signature);
  result = C_Sign(session,
                  data, arraysize(data),
                  signature, &signature_length);
  LOG(INFO) << "C_Sign: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
}

// Generates a test key pair.
void GenerateKeyPair(CK_SESSION_HANDLE session,
                     int key_size_bits,
                     const string& label,
                     bool is_temp) {
  CK_MECHANISM mechanism;
  mechanism.mechanism = CKM_RSA_PKCS_KEY_PAIR_GEN;
  mechanism.pParameter = NULL;
  mechanism.ulParameterLen = 0;
  CK_ULONG bits = key_size_bits;
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
    {CKA_PUBLIC_EXPONENT, e, sizeof(e)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)},
    {CKA_LABEL, const_cast<char*>(label.c_str()), label.length()},
  };
  CK_ATTRIBUTE private_attributes[] = {
    {CKA_DECRYPT, &true_value, sizeof(true_value)},
    {CKA_SIGN, &true_value, sizeof(true_value)},
    {CKA_UNWRAP, &false_value, sizeof(false_value)},
    {CKA_SENSITIVE, &true_value, sizeof(true_value)},
    {CKA_TOKEN, &true_value, sizeof(true_value)},
    {CKA_PRIVATE, &true_value, sizeof(true_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)},
    {CKA_LABEL, const_cast<char*>(label.c_str()), label.length()},
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

string bn2bin(BIGNUM* bn) {
  string bin;
  bin.resize(BN_num_bytes(bn));
  bin.resize(BN_bn2bin(bn, ConvertStringToByteBuffer(bin.data())));
  return bin;
}

// Generates a test key pair locally and injects it.
void InjectKeyPair(CK_SESSION_HANDLE session,
                   int key_size_bits,
                   const string& label) {
  RSA* rsa = RSA_generate_key(key_size_bits, 0x10001, NULL, NULL);
  if (!rsa) {
    LOG(ERROR) << "Failed to locally generate key pair.";
    exit(-1);
  }
  string n = bn2bin(rsa->n);
  string d = bn2bin(rsa->d);
  string p = bn2bin(rsa->p);
  string q = bn2bin(rsa->q);
  string dmp1 = bn2bin(rsa->dmp1);
  string dmq1 = bn2bin(rsa->dmq1);
  string iqmp = bn2bin(rsa->iqmp);
  RSA_free(rsa);
  CK_ULONG bits = key_size_bits;
  CK_BYTE e[] = {1, 0, 1};
  CK_BBOOL false_value = CK_FALSE;
  CK_BBOOL true_value = CK_TRUE;
  CK_OBJECT_CLASS pub_class = CKO_PUBLIC_KEY;
  CK_OBJECT_CLASS priv_class = CKO_PRIVATE_KEY;
  CK_KEY_TYPE key_type = CKK_RSA;
  CK_ATTRIBUTE public_attributes[] = {
    {CKA_CLASS, &pub_class, sizeof(pub_class)},
    {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
    {CKA_ENCRYPT, &true_value, sizeof(true_value)},
    {CKA_VERIFY, &true_value, sizeof(true_value)},
    {CKA_WRAP, &false_value, sizeof(false_value)},
    {CKA_TOKEN, &true_value, sizeof(true_value)},
    {CKA_PRIVATE, &false_value, sizeof(false_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)},
    {CKA_LABEL, const_cast<char*>(label.c_str()), label.length()},
    {CKA_MODULUS_BITS, &bits, sizeof(bits)},
    {CKA_PUBLIC_EXPONENT, e, sizeof(e)},
    {CKA_MODULUS, const_cast<char*>(n.c_str()), n.length()},
  };
  CK_ATTRIBUTE private_attributes[] = {
    {CKA_CLASS, &priv_class, sizeof(priv_class)},
    {CKA_KEY_TYPE, &key_type, sizeof(key_type)},
    {CKA_DECRYPT, &true_value, sizeof(true_value)},
    {CKA_SIGN, &true_value, sizeof(true_value)},
    {CKA_UNWRAP, &false_value, sizeof(false_value)},
    {CKA_SENSITIVE, &true_value, sizeof(true_value)},
    {CKA_TOKEN, &true_value, sizeof(true_value)},
    {CKA_PRIVATE, &true_value, sizeof(true_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)},
    {CKA_LABEL, const_cast<char*>(label.c_str()), label.length()},
    {CKA_PUBLIC_EXPONENT, e, sizeof(e)},
    {CKA_MODULUS, const_cast<char*>(n.c_str()), n.length()},
    {CKA_PRIVATE_EXPONENT, const_cast<char*>(d.c_str()), d.length()},
    {CKA_PRIME_1, const_cast<char*>(p.c_str()), p.length()},
    {CKA_PRIME_2, const_cast<char*>(q.c_str()), q.length()},
    {CKA_EXPONENT_1, const_cast<char*>(dmp1.c_str()), dmp1.length()},
    {CKA_EXPONENT_2, const_cast<char*>(dmq1.c_str()), dmq1.length()},
    {CKA_COEFFICIENT, const_cast<char*>(iqmp.c_str()), iqmp.length()},
  };
  CK_OBJECT_HANDLE public_key_handle = 0;
  CK_RV result = C_CreateObject(session,
                                public_attributes,
                                arraysize(public_attributes),
                                &public_key_handle);
  LOG(INFO) << "C_CreateObject: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
  CK_OBJECT_HANDLE private_key_handle = 0;
  result = C_CreateObject(session,
                          private_attributes,
                          arraysize(private_attributes),
                          &private_key_handle);
  LOG(INFO) << "C_CreateObject: " << chaps::CK_RVToString(result);
  if (result != CKR_OK)
    exit(-1);
}

// Deletes all test keys previously created.
void DeleteAllTestKeys(CK_SESSION_HANDLE session) {
  CK_OBJECT_CLASS class_value = CKO_PRIVATE_KEY;
  CK_ATTRIBUTE attributes[] = {
    {CKA_CLASS, &class_value, sizeof(class_value)},
    {CKA_ID, const_cast<char*>(kKeyID), strlen(kKeyID)}
  };
  vector<CK_OBJECT_HANDLE> objects;
  Find(session, attributes, arraysize(attributes), &objects);
  class_value = CKO_PUBLIC_KEY;
  Find(session, attributes, arraysize(attributes), &objects);
  for (size_t i = 0; i < objects.size(); ++i) {
    CK_RV result = C_DestroyObject(session, objects[i]);
    LOG(INFO) << "C_DestroyObject: " << chaps::CK_RVToString(result);
    if (result != CKR_OK)
      exit(-1);
  }
}

// Cleans up the session and library.
void TearDown(CK_SESSION_HANDLE session, bool logout) {
  CK_RV result = CKR_OK;
  if (logout) {
    result = C_Logout(session);
    LOG(INFO) << "C_Logout: " << chaps::CK_RVToString(result);
  }
  result = C_CloseSession(session);
  LOG(INFO) << "C_CloseSession: " << chaps::CK_RVToString(result);
  result = C_Finalize(NULL);
  LOG(INFO) << "C_Finalize: " << chaps::CK_RVToString(result);
}

void PrintHelp() {
  printf("Usage: p11_replay [COMMAND]\n");
  printf("Commands:\n");
  printf("  --generate [--label=<key_label> --key_size=<size_in_bits>]"
         " : Generates a key pair suitable for replay tests.\n");
  printf("  --inject [--label=<key_label> --key_size=<size_in_bits>]"
         " : Locally generates a key pair suitable for replay tests and injects"
         " it into the token.\n");
  printf("  --generate_delete : Generates a key pair and deletes it. This is "
         "useful for comparing key generation on different TPM models.\n");
  printf("  --list_objects : Lists all token objects.\n");
  printf("  --replay_vpn [--label=<key_label>]"
         " : Replays a L2TP/IPSEC VPN negotiation.\n");
  printf("  --replay_wifi [--label=<key_label>]"
         " : Replays a EAP-TLS Wifi negotiation. This is the default command if"
         " no command is specified.\n");
  printf("  --logout : Logs out once all other commands have finished.\n");
  printf("  --cleanup : Deletes all test keys.\n");
}

void PrintTicks(base::TimeTicks* start_ticks) {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delta = now - *start_ticks;
  *start_ticks = now;
  long long int value = delta.InMillisecondsRoundedUp();
  printf("Elapsed: %lldms\n", value);
}

void PrintObjects(const vector<CK_OBJECT_HANDLE>& objects) {
  for (size_t i = 0; i < objects.size(); ++i) {
    if (i > 0)
      printf(", ");
    printf("%d", static_cast<int>(objects[i]));
  }
  printf("\n");
}

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch("h") || cl->HasSwitch("help")) {
    PrintHelp();
    return 0;
  }
  bool generate = cl->HasSwitch("generate");
  bool inject = cl->HasSwitch("inject");
  bool generate_delete = cl->HasSwitch("generate_delete");
  bool vpn = cl->HasSwitch("replay_vpn");
  bool wifi = cl->HasSwitch("replay_wifi") || (cl->GetSwitches().size() == 0);
  bool logout = cl->HasSwitch("logout");
  bool cleanup = cl->HasSwitch("cleanup");
  bool list_objects = cl->HasSwitch("list_objects");
  if (!generate && !generate_delete && !vpn && !wifi && !logout && !cleanup &&
      !inject && !list_objects) {
    PrintHelp();
    return 0;
  }

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  base::TimeTicks start_ticks = base::TimeTicks::Now();
  CK_SLOT_ID slot = Initialize();
  CK_SESSION_HANDLE session = OpenSession(slot);
  PrintTicks(&start_ticks);
  string label = "_default";
  if (cl->HasSwitch("label"))
    label = cl->GetSwitchValueASCII("label");
  int key_size_bits = 2048;
  if (cl->HasSwitch("key_size") &&
      !base::StringToInt(cl->GetSwitchValueASCII("key_size"), &key_size_bits))
    key_size_bits = 2048;
  if (generate || generate_delete) {
    session = Login(slot, false, session);
    PrintTicks(&start_ticks);
    GenerateKeyPair(session, key_size_bits, label, generate_delete);
    PrintTicks(&start_ticks);
  } else if (inject) {
    InjectKeyPair(session, key_size_bits, label);
  }
  if (list_objects) {
    vector<CK_OBJECT_HANDLE> objects;
    CK_BBOOL priv_value = CK_FALSE;
    CK_ATTRIBUTE priv = {CKA_PRIVATE, &priv_value, sizeof(priv_value)};
    Find(session, &priv, 1, &objects);
    printf("Public Objects:\n");
    PrintObjects(objects);
    PrintTicks(&start_ticks);
    objects.clear();
    Login(slot, false, session);
    priv_value = CK_TRUE;
    Find(session, &priv, 1, &objects);
    printf("Private Objects:\n");
    PrintObjects(objects);
    PrintTicks(&start_ticks);
  }
  if (vpn || wifi) {
    printf("Replay 1 of 2\n");
    session = Login(slot, vpn, session);
    Sign(session, label);
    PrintTicks(&start_ticks);
    printf("Replay 2 of 2\n");
    CK_SESSION_HANDLE session2 = OpenSession(slot);
    session2 = Login(slot, vpn, session2);
    Sign(session2, label);
    PrintTicks(&start_ticks);
    C_CloseSession(session2);
  }
  if (cleanup)
    DeleteAllTestKeys(session);
  TearDown(session, logout);
  PrintTicks(&start_ticks);
}
