// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/slot_manager_impl.h"

#include <limits.h>
#include <string.h>

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "chaps/chaps_utility.h"
#include "chaps/session.h"
#include "chaps/tpm_utility.h"
#include "pkcs11/cryptoki.h"

using std::map;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chaps {

namespace {

// I18N Note: The descriptive strings are needed for PKCS #11 compliance but
// they should not appear on any UI.
const FilePath::CharType kDefaultTokenFile[] = FILE_PATH_LITERAL("token");
const CK_VERSION kDefaultVersion = {1, 0};
const char kManufacturerID[] = "Chromium OS";
const CK_ULONG kMaxPinLen = 127;
const CK_ULONG kMinPinLen = 6;
const char kSlotDescription[] = "TPM Slot";
const FilePath::CharType kSystemTokenPath[] =
    FILE_PATH_LITERAL("/opt/google/chaps");
const char kSystemTokenAuthData[] = "000000";
const int kSystemTokenSlot = 0;
const char kTokenLabel[] = "User-Specific TPM Token";
const char kTokenModel[] = "";
const char kTokenSerialNumber[] = "Not Available";
const int kUserKeySize = 32;

const struct MechanismInfo {
  CK_MECHANISM_TYPE type;
  CK_MECHANISM_INFO info;
} kDefaultMechanismInfo[] = {
  {CKM_RSA_PKCS_KEY_PAIR_GEN, {512, 2048, CKF_GENERATE_KEY_PAIR | CKF_HW}},
  {CKM_RSA_PKCS, {512, 2048, CKF_HW | CKF_ENCRYPT | CKF_DECRYPT | CKF_SIGN |
      CKF_VERIFY}},
  {CKM_MD5_RSA_PKCS, {512, 2048, CKF_HW | CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA1_RSA_PKCS, {512, 2048, CKF_HW | CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA256_RSA_PKCS, {512, 2048, CKF_HW | CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA384_RSA_PKCS, {512, 2048, CKF_HW | CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA512_RSA_PKCS, {512, 2048, CKF_HW | CKF_SIGN | CKF_VERIFY}},
  {CKM_MD5, {0, 0, CKF_DIGEST}},
  {CKM_SHA_1, {0, 0, CKF_DIGEST}},
  {CKM_SHA256, {0, 0, CKF_DIGEST}},
  {CKM_SHA384, {0, 0, CKF_DIGEST}},
  {CKM_SHA512, {0, 0, CKF_DIGEST}},
  {CKM_GENERIC_SECRET_KEY_GEN, {8, 1024, CKF_GENERATE}},
  {CKM_MD5_HMAC, {0, 0, CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA_1_HMAC, {0, 0, CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA256_HMAC, {0, 0, CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA512_HMAC, {0, 0, CKF_SIGN | CKF_VERIFY}},
  {CKM_SHA384_HMAC, {0, 0, CKF_SIGN | CKF_VERIFY}},
  {CKM_DES_KEY_GEN, {0, 0, CKF_GENERATE}},
  {CKM_DES_ECB, {0, 0, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_DES_CBC, {0, 0, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_DES_CBC_PAD, {0, 0, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_DES3_KEY_GEN, {0, 0, CKF_GENERATE}},
  {CKM_DES3_ECB, {0, 0, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_DES3_CBC, {0, 0, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_DES3_CBC_PAD, {0, 0, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_AES_KEY_GEN, {16, 32, CKF_GENERATE}},
  {CKM_AES_ECB, {16, 32, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_AES_CBC, {16, 32, CKF_ENCRYPT | CKF_DECRYPT}},
  {CKM_AES_CBC_PAD, {16, 32, CKF_ENCRYPT | CKF_DECRYPT}}
};

}  // namespace

SlotManagerImpl::SlotManagerImpl(ChapsFactory* factory, TPMUtility* tpm_utility)
    : factory_(factory),
      last_handle_(0),
      tpm_utility_(tpm_utility) {
  CHECK(factory_);
  CHECK(tpm_utility_);
}

SlotManagerImpl::~SlotManagerImpl() {
  for (size_t i = 0; i < slot_list_.size(); ++i) {
    // Unload any keys that have been loaded in the TPM.
    tpm_utility_->UnloadKeysForSlot(i);
  }
}

bool SlotManagerImpl::Init() {
  // Populate mechanism info.  This will be the same for all TPM-backed tokens.
  for (size_t i = 0; i < arraysize(kDefaultMechanismInfo); ++i) {
    mechanism_info_[kDefaultMechanismInfo[i].type] =
        kDefaultMechanismInfo[i].info;
  }
  // Mix in some random bytes from the TPM to the openssl prng.
  // TODO(dkrahn): crosbug.com/25435 - Evaluate whether to use the TPM RNG.
  string random;
  if (tpm_utility_->GenerateRandom(128, &random))
    RAND_seed(ConvertStringToByteBuffer(random.data()), random.length());
  // Default semantics are to always start with two slots.  One 'system' slot
  // which always has a token available, and one 'user' slot which will have no
  // token until a login event is received.
  // TODO(dkrahn): Make this 2 once we're ready to enable the system token.
  // crosbug.com/27759.
  AddSlots(1);
  // Setup the system token.  This is the same as for a user token so we can
  // just do what we normally do when a user logs in.  We'll know it succeeded
  // if the system token slot has a token inserted.
  // TODO(dkrahn): Uncomment once we're ready to enable the system token.
  // crosbug.com/27759.
  //OnLogin(FilePath(kSystemTokenPath), kSystemTokenAuthData);
  //return IsTokenPresent(kSystemTokenSlot);
  return true;
}

int SlotManagerImpl::GetSlotCount() const {
  return slot_list_.size();
}

bool SlotManagerImpl::IsTokenPresent(int slot_id) const {
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());

  return ((slot_list_[slot_id].slot_info.flags & CKF_TOKEN_PRESENT) ==
      CKF_TOKEN_PRESENT);
}

void SlotManagerImpl::GetSlotInfo(int slot_id, CK_SLOT_INFO* slot_info) const {
  CHECK(slot_info);
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());

  *slot_info = slot_list_[slot_id].slot_info;
}

void SlotManagerImpl::GetTokenInfo(int slot_id,
                                   CK_TOKEN_INFO* token_info) const {
  CHECK(token_info);
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());
  CHECK(IsTokenPresent(slot_id));

  *token_info = slot_list_[slot_id].token_info;
}

const MechanismMap* SlotManagerImpl::GetMechanismInfo(int slot_id) const {
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());
  CHECK(IsTokenPresent(slot_id));

  return &mechanism_info_;
}

int SlotManagerImpl::OpenSession(int slot_id, bool is_read_only) {
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());
  CHECK(IsTokenPresent(slot_id));

  shared_ptr<Session> session(factory_->CreateSession(
      slot_id,
      slot_list_[slot_id].token_object_pool.get(),
      tpm_utility_,
      this,
      is_read_only));
  CHECK(session.get());
  // If we use this many sessions, we have a problem.
  int session_id = CreateHandle();
  slot_list_[slot_id].sessions[session_id] = session;
  session_slot_map_[session_id] = slot_id;
  return session_id;
}

bool SlotManagerImpl::CloseSession(int session_id) {
  Session* session = NULL;
  if (!GetSession(session_id, &session))
    return false;
  CHECK(session);
  int slot_id = session_slot_map_[session_id];
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());
  session_slot_map_.erase(session_id);
  slot_list_[slot_id].sessions.erase(session_id);
  return true;
}

void SlotManagerImpl::CloseAllSessions(int slot_id) {
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());

  for (map<int, shared_ptr<Session> >::iterator iter =
          slot_list_[slot_id].sessions.begin();
       iter != slot_list_[slot_id].sessions.end();
       ++iter) {
    session_slot_map_.erase(iter->first);
  }
  slot_list_[slot_id].sessions.clear();
}

bool SlotManagerImpl::GetSession(int session_id, Session** session) const {
  CHECK(session);

  // Lookup which slot this session belongs to.
  map<int, int>::const_iterator session_slot_iter =
      session_slot_map_.find(session_id);
  if (session_slot_iter == session_slot_map_.end())
    return false;
  int slot_id = session_slot_iter->second;
  CHECK_LT(static_cast<size_t>(slot_id), slot_list_.size());

  // Lookup the session instance.
  map<int, shared_ptr<Session> >::const_iterator session_iter =
      slot_list_[slot_id].sessions.find(session_id);
  if (session_iter == slot_list_[slot_id].sessions.end())
    return false;
  *session = session_iter->second.get();
  return true;
}

void SlotManagerImpl::OnLogin(const FilePath& path, const string& auth_data) {
  // If we're already managing this token, ignore the event.
  if (path_slot_map_.find(path) != path_slot_map_.end()) {
    LOG(WARNING) << "Login event received for existing slot.";
    return;
  }
  // Setup the object pool and the key hierarchy.
  shared_ptr<ObjectPool> object_pool(
      factory_->CreateObjectPool(this,
          factory_->CreateObjectStore(path.Append(kDefaultTokenFile))));
  CHECK(object_pool.get());
  int slot_id = FindEmptySlot();
  string auth_key_blob;
  string encrypted_master_key;
  string master_key;
  if (!object_pool->GetInternalBlob(kEncryptedAuthKey, &auth_key_blob) ||
      !object_pool->GetInternalBlob(kEncryptedMasterKey,
                                    &encrypted_master_key)) {
    LOG(INFO) << "Initializing key hierarchy for token at " << path.value();
    if (!InitializeKeyHierarchy(slot_id,
                                object_pool.get(),
                                sha1(auth_data),
                                &master_key)) {
      LOG(ERROR) << "Failed to initialize key hierarchy at " << path.value();
      tpm_utility_->UnloadKeysForSlot(slot_id);
      return;
    }
  } else {
    if (!tpm_utility_->Authenticate(slot_id,
                                    sha1(auth_data),
                                    auth_key_blob,
                                    encrypted_master_key,
                                    &master_key)) {
      LOG(ERROR) << "Authentication failed for token at " << path.value();
      tpm_utility_->UnloadKeysForSlot(slot_id);
      return;
    }
  }
  object_pool->SetKey(master_key);
  // Insert the new token into the empty slot.
  slot_list_[slot_id].token_object_pool = object_pool;
  slot_list_[slot_id].slot_info.flags |= CKF_TOKEN_PRESENT;
  path_slot_map_[path] = slot_id;
  LOG(INFO) << "Key hierarchy ready for token at " << path.value();
}

void SlotManagerImpl::OnLogout(const FilePath& path) {
  // If we're not managing this token, ignore the event.
  if (path_slot_map_.find(path) == path_slot_map_.end()) {
    LOG(WARNING) << "Logout event received for unknown path: " << path.value();
    return;
  }
  int slot_id = path_slot_map_[path];
  tpm_utility_->UnloadKeysForSlot(slot_id);
  CloseAllSessions(slot_id);
  slot_list_[slot_id].token_object_pool.reset();
  slot_list_[slot_id].slot_info.flags &= ~CKF_TOKEN_PRESENT;
  path_slot_map_.erase(path);
}

void SlotManagerImpl::OnChangeAuthData(const FilePath& path,
                                       const string& old_auth_data,
                                       const string& new_auth_data) {
  // This event can be handled whether or not we are already managing the token
  // but if we're not, we won't start until a Login event comes in.
  ObjectPool* object_pool = NULL;
  scoped_ptr<ObjectPool> scoped_object_pool;
  int slot_id = 0;
  bool unload = false;
  if (path_slot_map_.find(path) == path_slot_map_.end()) {
    object_pool = factory_->CreateObjectPool(this, factory_->CreateObjectStore(
        path.Append(kDefaultTokenFile)));
    scoped_object_pool.reset(object_pool);
    slot_id = FindEmptySlot();
    unload = true;
  } else {
    slot_id = path_slot_map_[path];
    object_pool = slot_list_[slot_id].token_object_pool.get();
  }
  CHECK(object_pool);
  string auth_key_blob;
  string new_auth_key_blob;
  if (!object_pool->GetInternalBlob(kEncryptedAuthKey, &auth_key_blob)) {
    LOG(INFO) << "Token not initialized; ignoring change auth data event.";
  } else if (!tpm_utility_->ChangeAuthData(slot_id,
                                           sha1(old_auth_data),
                                           sha1(new_auth_data),
                                           auth_key_blob,
                                           &new_auth_key_blob)) {
    LOG(ERROR) << "Failed to change auth data for token at " << path.value();
  } else if (!object_pool->SetInternalBlob(kEncryptedAuthKey,
                                           new_auth_key_blob)) {
    LOG(ERROR) << "Failed to write changed auth blob for token at "
               << path.value();
  }
  if (unload)
    tpm_utility_->UnloadKeysForSlot(slot_id);
}

int SlotManagerImpl::CreateHandle() {
  // If we use this many handles, we have a problem.
  CHECK(last_handle_ < INT_MAX);
  return ++last_handle_;
}

void SlotManagerImpl::GetDefaultInfo(CK_SLOT_INFO* slot_info,
                                     CK_TOKEN_INFO* token_info) {
  memset(slot_info, 0, sizeof(CK_SLOT_INFO));
  CopyStringToCharBuffer(kSlotDescription,
                         slot_info->slotDescription,
                         sizeof(slot_info->slotDescription));
  CopyStringToCharBuffer(kManufacturerID,
                         slot_info->manufacturerID,
                         sizeof(slot_info->manufacturerID));
  slot_info->flags = CKF_HW_SLOT | CKF_REMOVABLE_DEVICE;
  slot_info->hardwareVersion = kDefaultVersion;
  slot_info->firmwareVersion = kDefaultVersion;

  memset(token_info, 0, sizeof(CK_TOKEN_INFO));
  CopyStringToCharBuffer(kTokenLabel,
                         token_info->label,
                         sizeof(token_info->label));
  CopyStringToCharBuffer(kManufacturerID,
                         token_info->manufacturerID,
                         sizeof(token_info->manufacturerID));
  CopyStringToCharBuffer(kTokenModel,
                         token_info->model,
                         sizeof(token_info->model));
  CopyStringToCharBuffer(kTokenSerialNumber,
                         token_info->serialNumber,
                         sizeof(token_info->serialNumber));
  token_info->flags = CKF_RNG |
                      CKF_USER_PIN_INITIALIZED |
                      CKF_PROTECTED_AUTHENTICATION_PATH |
                      CKF_TOKEN_INITIALIZED;
  token_info->ulMaxSessionCount = CK_EFFECTIVELY_INFINITE;
  token_info->ulSessionCount = CK_UNAVAILABLE_INFORMATION;
  token_info->ulMaxRwSessionCount = CK_EFFECTIVELY_INFINITE;
  token_info->ulRwSessionCount = CK_UNAVAILABLE_INFORMATION;
  token_info->ulMaxPinLen = kMaxPinLen;
  token_info->ulMinPinLen = kMinPinLen;
  token_info->ulTotalPublicMemory = CK_UNAVAILABLE_INFORMATION;
  token_info->ulFreePublicMemory = CK_UNAVAILABLE_INFORMATION;
  token_info->ulTotalPrivateMemory = CK_UNAVAILABLE_INFORMATION;
  token_info->ulFreePrivateMemory = CK_UNAVAILABLE_INFORMATION;
  token_info->hardwareVersion = kDefaultVersion;
  token_info->firmwareVersion = kDefaultVersion;
}

bool SlotManagerImpl::InitializeKeyHierarchy(int slot_id,
                                             ObjectPool* object_pool,
                                             const string& auth_data,
                                             string* master_key) {
  if (!tpm_utility_->GenerateRandom(kUserKeySize, master_key)) {
    LOG(ERROR) << "Failed to generate user encryption key.";
    return false;
  }
  string auth_key_blob;
  int auth_key_handle;
  const int key_size = 2048;
  const string public_exponent("\x01\x00\x01", 3);
  if (!tpm_utility_->GenerateKey(slot_id,
                                 key_size,
                                 public_exponent,
                                 auth_data,
                                 &auth_key_blob,
                                 &auth_key_handle)) {
    LOG(ERROR) << "Failed to generate user authentication key.";
    return false;
  }
  string encrypted_master_key;
  if (!tpm_utility_->Bind(auth_key_handle,
                          *master_key,
                          &encrypted_master_key)) {
    LOG(ERROR) << "Failed to bind user encryption key.";
    return false;
  }
  if (!object_pool->SetInternalBlob(kEncryptedAuthKey, auth_key_blob) ||
      !object_pool->SetInternalBlob(kEncryptedMasterKey,
                                    encrypted_master_key)) {
    LOG(ERROR) << "Failed to write key hierarchy blobs.";
    return false;
  }
  return true;
}

int SlotManagerImpl::FindEmptySlot() {
  size_t i = 0;
  for (; i < slot_list_.size(); ++i) {
    if (!IsTokenPresent(i))
      return i;
  }
  // Add a new slot.
  AddSlots(1);
  return i;
}

void SlotManagerImpl::AddSlots(int num_slots) {
  for (int i = 0; i < num_slots; ++i) {
    Slot slot;
    GetDefaultInfo(&slot.slot_info, &slot.token_info);
    LOG(INFO) << "Adding slot: " << slot_list_.size();
    slot_list_.push_back(slot);
  }
}

}  // namespace
