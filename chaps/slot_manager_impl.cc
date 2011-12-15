// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/slot_manager_impl.h"

#include <limits.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>

#include "chaps/chaps_utility.h"
#include "chaps/session.h"
#include "chaps/tpm_utility.h"
#include "pkcs11/cryptoki.h"

using std::map;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chaps {

// I18N Note: The descriptive strings are needed for PKCS #11 compliance but
// they should not appear on any UI.
static const char* kDefaultTokenFile = "token";
static const CK_VERSION kDefaultVersion = {1, 0};
static const char* kManufacturerID = "Chromium OS";
static const CK_ULONG kMaxPinLen = 127;
static const CK_ULONG kMinPinLen = 6;
static const char* kSlotDescription = "TPM Slot";
static const char* kSystemTokenPath = "/opt/google/chaps";
static const char* kSystemTokenAuthData = "000000";
static const char* kTokenLabel = "TPM Token";
static const char* kTokenModel = "";
static const char* kTokenSerialNumber = "Not Available";
static const int kUserKeySize = 32;

static const struct MechanismInfo {
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

SlotManagerImpl::SlotManagerImpl(ChapsFactory* factory, TPMUtility* tpm_utility)
    : factory_(factory),
      last_session_id_(0),
      tpm_utility_(tpm_utility) {}

SlotManagerImpl::~SlotManagerImpl() {
}

bool SlotManagerImpl::Init() {
  CHECK(factory_);
  CHECK(tpm_utility_);
  // Populate mechanism info.  This will be the same for all TPM-backed tokens.
  for (size_t i = 0; i < arraysize(kDefaultMechanismInfo); ++i) {
    mechanism_info_[kDefaultMechanismInfo[i].type] =
        kDefaultMechanismInfo[i].info;
  }
  // Default semantics are to always start with two slots.  One 'system' slot
  // which always has a token available, and one 'user' slot which will have no
  // token until a login event is received.
  AddSlots(2);
  // Setup the system token.  This is the same as for a user token so we can
  // just do what we normally do when a user logs in.  We'll know it succeeded
  // if slot 0 has a token inserted.
  OnLogin(kSystemTokenPath, kSystemTokenAuthData);
  return IsTokenPresent(0);
}

int SlotManagerImpl::GetSlotCount() const {
  return slot_list_.size();
}

bool SlotManagerImpl::IsTokenPresent(int slot_id) const {
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  return ((slot_list_[slot_id].slot_info_.flags & CKF_TOKEN_PRESENT) ==
      CKF_TOKEN_PRESENT);
}

void SlotManagerImpl::GetSlotInfo(int slot_id, CK_SLOT_INFO* slot_info) const {
  CHECK(slot_info);
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  memcpy(slot_info, &slot_list_[slot_id].slot_info_, sizeof(CK_SLOT_INFO));
}

void SlotManagerImpl::GetTokenInfo(int slot_id,
                                   CK_TOKEN_INFO* token_info) const {
  CHECK(token_info);
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  CHECK(IsTokenPresent(slot_id));
  memcpy(token_info, &slot_list_[slot_id].token_info_, sizeof(CK_TOKEN_INFO));
}

const MechanismMap* SlotManagerImpl::GetMechanismInfo(int slot_id) const {
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  CHECK(IsTokenPresent(slot_id));
  return &mechanism_info_;
}

int SlotManagerImpl::OpenSession(int slot_id, bool is_read_only) {
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  CHECK(IsTokenPresent(slot_id));
  shared_ptr<Session> session(factory_->CreateSession(
      slot_id,
      slot_list_[slot_id].token_object_pool_.get(),
      tpm_utility_,
      is_read_only));
  CHECK(session.get());
  // If we use this many sessions, we have a problem.
  CHECK(last_session_id_ < INT_MAX);
  int session_id = ++last_session_id_;
  slot_list_[slot_id].sessions_[session_id] = session;
  session_slot_map_[session_id] = slot_id;
  return session_id;
}

bool SlotManagerImpl::CloseSession(int session_id) {
  Session* session = NULL;
  if (!GetSession(session_id, &session))
    return false;
  CHECK(session);
  int slot_id = session_slot_map_[session_id];
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  session_slot_map_.erase(session_id);
  slot_list_[slot_id].sessions_.erase(session_id);
  return true;
}

void SlotManagerImpl::CloseAllSessions(int slot_id) {
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  map<int, shared_ptr<Session> >::iterator it;
  for (it = slot_list_[slot_id].sessions_.begin();
       it != slot_list_[slot_id].sessions_.end();
       ++it) {
    session_slot_map_.erase(it->first);
  }
  slot_list_[slot_id].sessions_.clear();
}

bool SlotManagerImpl::GetSession(int session_id, Session** session) const {
  CHECK(session);
  // Lookup which slot this session belongs to.
  map<int, int>::const_iterator session_slot_it =
      session_slot_map_.find(session_id);
  if (session_slot_it == session_slot_map_.end())
    return false;
  int slot_id = session_slot_it->second;
  CHECK(static_cast<size_t>(slot_id) < slot_list_.size());
  // Lookup the session instance.
  map<int, shared_ptr<Session> >::const_iterator session_it =
      slot_list_[slot_id].sessions_.find(session_id);
  if (session_it == slot_list_[slot_id].sessions_.end())
    return false;
  *session = session_it->second.get();
  return true;
}

void SlotManagerImpl::OnLogin(const string& path, const string& auth_data) {
  // If we're already managing this token, ignore the event.
  if (path_slot_map_.find(path) != path_slot_map_.end()) {
    LOG(WARNING) << "Login event received for existing slot.";
    return;
  }
  // Setup the object pool and the key hierarchy.
  shared_ptr<ObjectPool> object_pool(factory_->CreatePersistentObjectPool(
      path + "/" + kDefaultTokenFile));
  CHECK(object_pool.get());
  string auth_key_blob;
  string encrypted_master_key;
  string master_key;
  if (!object_pool->GetInternalBlob(kEncryptedAuthKey, &auth_key_blob) ||
      !object_pool->GetInternalBlob(kEncryptedMasterKey,
                                    &encrypted_master_key)) {
    LOG(INFO) << "Initializing key hierarchy for token at " << path;
    if (!InitializeKeyHierarchy(object_pool.get(),
                                auth_data,
                                &master_key)) {
      LOG(ERROR) << "Failed to initialize key hierarchy at " << path;
      return;
    }
  } else {
    if (!tpm_utility_->Authenticate(auth_data,
                                    auth_key_blob,
                                    encrypted_master_key,
                                    &master_key)) {
      LOG(ERROR) << "Authentication failed for token at " << path;
      return;
    }
  }
  object_pool->SetKey(master_key);
  // Insert the new token into a slot.
  int slot_id = FindEmptySlot();
  slot_list_[slot_id].token_object_pool_ = object_pool;
  slot_list_[slot_id].slot_info_.flags |= CKF_TOKEN_PRESENT;
  path_slot_map_[path] = slot_id;
}

void SlotManagerImpl::OnLogout(const string& path) {
  // If we're not managing this token, ignore the event.
  if (path_slot_map_.find(path) == path_slot_map_.end()) {
    LOG(WARNING) << "Logout event received for unknown path: " << path;
    return;
  }
  int slot_id = path_slot_map_[path];
  CloseAllSessions(slot_id);
  slot_list_[slot_id].token_object_pool_.reset();
  slot_list_[slot_id].slot_info_.flags &= ~CKF_TOKEN_PRESENT;
  path_slot_map_.erase(path);
}

void SlotManagerImpl::OnChangeAuthData(const string& path,
                                       const string& old_auth_data,
                                       const string& new_auth_data) {
  // This event can be handled whether or not we are already managing the token
  // but if we're not, we won't start until a Login event comes in.
  ObjectPool* object_pool = NULL;
  scoped_ptr<ObjectPool> scoped_object_pool;
  if (path_slot_map_.find(path) == path_slot_map_.end()) {
    object_pool = factory_->CreatePersistentObjectPool(
        path + "/" + kDefaultTokenFile);
    scoped_object_pool.reset(object_pool);
  } else {
    int slot_id = path_slot_map_[path];
    object_pool = slot_list_[slot_id].token_object_pool_.get();
  }
  CHECK(object_pool);
  string auth_key_blob;
  if (!object_pool->GetInternalBlob(kEncryptedAuthKey, &auth_key_blob)) {
    LOG(INFO) << "Token not initialized; ignoring change auth data event.";
    return;
  }
  string new_auth_key_blob;
  if (!tpm_utility_->ChangeAuthData(old_auth_data,
                                    new_auth_data,
                                    auth_key_blob,
                                    &new_auth_key_blob)) {
    LOG(ERROR) << "Failed to change auth data for token at " << path;
    return;
  }
  if (!object_pool->SetInternalBlob(kEncryptedAuthKey, new_auth_key_blob)) {
    LOG(ERROR) << "Failed to write changed auth blob for token at " << path;
    return;
  }
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

bool SlotManagerImpl::InitializeKeyHierarchy(ObjectPool* object_pool,
                                             const string& auth_data,
                                             string* master_key) {
  if (!tpm_utility_->GenerateRandom(kUserKeySize, master_key)) {
    LOG(ERROR) << "Failed to generate user encryption key.";
    return false;
  }
  string auth_key_blob;
  if (!tpm_utility_->GenerateKey(auth_data, &auth_key_blob)) {
    LOG(ERROR) << "Failed to generate user authentication key.";
    return false;
  }
  string encrypted_master_key;
  if (!tpm_utility_->Bind(auth_key_blob,
                          auth_data,
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
    GetDefaultInfo(&slot.slot_info_, &slot.token_info_);
    slot_list_.push_back(slot);
  }
}

}  // namespace
