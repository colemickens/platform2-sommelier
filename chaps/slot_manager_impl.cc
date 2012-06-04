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
#include <base/memory/scoped_ptr.h>
#include <chromeos/utility.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "chaps/chaps_utility.h"
#include "chaps/object_importer.h"
#include "chaps/session.h"
#include "chaps/tpm_utility.h"
#include "pkcs11/cryptoki.h"

using base::AutoLock;
using std::map;
using std::string;
using std::tr1::shared_ptr;
using std::vector;

namespace chaps {

namespace {

// I18N Note: The descriptive strings are needed for PKCS #11 compliance but
// they should not appear on any UI.
const CK_VERSION kDefaultVersion = {1, 0};
const char kManufacturerID[] = "Chromium OS";
const CK_ULONG kMaxPinLen = 127;
const CK_ULONG kMinPinLen = 6;
const char kSlotDescription[] = "TPM Slot";
const FilePath::CharType kSystemTokenPath[] =
    FILE_PATH_LITERAL("/var/lib/chaps");
const char kSystemTokenAuthData[] = "000000";
const int kSystemTokenSlot = 0;
const char kTokenLabel[] = "User-Specific TPM Token";
const char kTokenModel[] = "";
const char kTokenSerialNumber[] = "Not Available";
const int kUserKeySize = 32;
const int kAuthDataHashVersion = 1;

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

// Computes an authorization data hash as it is stored in the database.
string HashAuthData(const string& auth_data) {
  string version(1, kAuthDataHashVersion);
  string hash = Sha512(auth_data);
  string hash_byte = hash.substr(0, 1);
  chromeos::SecureMemset(const_cast<char*>(hash.data()), 0, hash.length());
  return version + hash_byte;
}

// Sanity checks authorization data by comparing against a hash stored in the
// token database.
// Args:
//   auth_data_hash - A hash of the authorization data to be verified.
//   saved_auth_data_hash - The hash currently stored in the database.
// Returns:
//   False if both hash values are valid and they do not match.
bool SanityCheckAuthData(const string& auth_data_hash,
                         const string& saved_auth_data_hash) {
  CHECK(auth_data_hash.length() == 2);
  if (saved_auth_data_hash.length() != 2 ||
      saved_auth_data_hash[0] != kAuthDataHashVersion)
    return true;
  return (auth_data_hash[1] == saved_auth_data_hash[1]);
}

// Performs expensive tasks required to initialize a token.
class TokenInitThread : public base::PlatformThread::Delegate {
 public:
  // This class will not take ownership of any pointers.
  TokenInitThread(int slot_id,
                  FilePath path,
                  const string& auth_data,
                  TPMUtility* tpm_utility,
                  ObjectPool* object_pool);

  virtual ~TokenInitThread() {}

  // PlatformThread::Delegate interface.
  void ThreadMain();

 private:
  bool InitializeKeyHierarchy(string* master_key);

  int slot_id_;
  FilePath path_;
  string auth_data_;
  TPMUtility* tpm_utility_;
  ObjectPool* object_pool_;
};

// Performs expensive tasks required to terminate a token.
class TokenTermThread : public base::PlatformThread::Delegate {
 public:
  // This class will not take ownership of any pointers.
  TokenTermThread(int slot_id, TPMUtility* tpm_utility)
      : slot_id_(slot_id),
        tpm_utility_(tpm_utility) {}

  virtual ~TokenTermThread() {}

  // PlatformThread::Delegate interface.
  void ThreadMain() {
    tpm_utility_->UnloadKeysForSlot(slot_id_);
  }

 private:
  int slot_id_;
  TPMUtility* tpm_utility_;
};

TokenInitThread::TokenInitThread(int slot_id,
                                 FilePath path,
                                 const string& auth_data,
                                 TPMUtility* tpm_utility,
                                 ObjectPool* object_pool)
    : slot_id_(slot_id),
      path_(path),
      auth_data_(auth_data),
      tpm_utility_(tpm_utility),
      object_pool_(object_pool) {}

void TokenInitThread::ThreadMain() {
  string auth_data_hash = HashAuthData(auth_data_);
  string saved_auth_data_hash;
  string auth_key_blob;
  string encrypted_master_key;
  string master_key;
  // Determine whether the key hierarchy has already been initialized based on
  // whether the relevant blobs exist.
  if (!object_pool_->GetInternalBlob(kEncryptedAuthKey, &auth_key_blob) ||
      !object_pool_->GetInternalBlob(kEncryptedMasterKey,
                                     &encrypted_master_key)) {
    LOG(INFO) << "Initializing key hierarchy for token at " << path_.value();
    if (!InitializeKeyHierarchy(&master_key)) {
      LOG(ERROR) << "Failed to initialize key hierarchy at " << path_.value();
      tpm_utility_->UnloadKeysForSlot(slot_id_);
    }
  } else {
    // Don't send the auth data to the TPM if it fails to verify against the
    // saved hash.
    object_pool_->GetInternalBlob(kAuthDataHash, &saved_auth_data_hash);
    if (!SanityCheckAuthData(auth_data_hash, saved_auth_data_hash) ||
        !tpm_utility_->Authenticate(slot_id_,
                                    Sha1(auth_data_),
                                    auth_key_blob,
                                    encrypted_master_key,
                                    &master_key)) {
      LOG(ERROR) << "Authentication failed for token at " << path_.value()
                 << ", reinitializing token.";
      tpm_utility_->UnloadKeysForSlot(slot_id_);
      if (!object_pool_->DeleteAll())
        LOG(WARNING) << "Failed to delete all existing objects.";
      if (!InitializeKeyHierarchy(&master_key)) {
        LOG(ERROR) << "Failed to initialize key hierarchy at " << path_.value();
        tpm_utility_->UnloadKeysForSlot(slot_id_);
      }
    }
  }
  if (!object_pool_->SetEncryptionKey(master_key)) {
    LOG(ERROR) << "SetEncryptionKey failed for token at " << path_.value();
    tpm_utility_->UnloadKeysForSlot(slot_id_);
    return;
  }
  if (!master_key.empty()) {
    if (auth_data_hash != saved_auth_data_hash)
      object_pool_->SetInternalBlob(kAuthDataHash, auth_data_hash);
    LOG(INFO) << "Master key is ready for token at " << path_.value();
  }
}

bool TokenInitThread::InitializeKeyHierarchy(string* master_key) {
  if (!tpm_utility_->GenerateRandom(kUserKeySize, master_key)) {
    LOG(ERROR) << "Failed to generate user encryption key.";
    return false;
  }
  string auth_key_blob;
  int auth_key_handle;
  const int key_size = 2048;
  const string public_exponent("\x01\x00\x01", 3);
  if (!tpm_utility_->GenerateKey(slot_id_,
                                 key_size,
                                 public_exponent,
                                 Sha1(auth_data_),
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
  if (!object_pool_->SetInternalBlob(kEncryptedAuthKey, auth_key_blob) ||
      !object_pool_->SetInternalBlob(kEncryptedMasterKey,
                                    encrypted_master_key)) {
    LOG(ERROR) << "Failed to write key hierarchy blobs.";
    return false;
  }
  return true;
}

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
    // Wait for any worker thread to finish.
    if (slot_list_[i].worker_thread.get())
      base::PlatformThread::Join(slot_list_[i].worker_thread_handle);
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
  string random;
  if (tpm_utility_->GenerateRandom(128, &random)) {
    RAND_seed(ConvertStringToByteBuffer(random.data()), random.length());
  } else {
    LOG(WARNING) << "TPM failed to generate random data.";
  }
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
  VLOG(1) << "SlotManagerImpl::OnLogin enter";
  // If we're already managing this token, ignore the event.
  if (path_slot_map_.find(path) != path_slot_map_.end()) {
    LOG(WARNING) << "Login event received for existing slot.";
    return;
  }
  // If there's something wrong with the TPM, don't attempt to load a token.
  if (!tpm_utility_->Init()) {
    LOG(ERROR) << "Failed to initialize TPM, login event aborting.";
    return;
  }
  // Setup the object pool.
  int slot_id = FindEmptySlot();
  shared_ptr<ObjectPool> object_pool(
      factory_->CreateObjectPool(this,
                                 factory_->CreateObjectStore(path),
                                 factory_->CreateObjectImporter(slot_id,
                                                                path,
                                                                tpm_utility_)));
  CHECK(object_pool.get());

  // Wait for the termination of a previous token.
  if (slot_list_[slot_id].worker_thread.get())
    base::PlatformThread::Join(slot_list_[slot_id].worker_thread_handle);

  // Decrypting (or creating) the master key requires the TPM so we'll put this
  // on a worker thread. This has the effect that queries for public objects
  // are responsive but queries for private objects will be waiting for the
  // master key to be ready.
  slot_list_[slot_id].worker_thread.reset(
      new TokenInitThread(slot_id,
                          path,
                          auth_data,
                          tpm_utility_,
                          object_pool.get()));
  base::PlatformThread::Create(0,
                               slot_list_[slot_id].worker_thread.get(),
                               &slot_list_[slot_id].worker_thread_handle);

  // Insert the new token into the empty slot.
  slot_list_[slot_id].token_object_pool = object_pool;
  slot_list_[slot_id].slot_info.flags |= CKF_TOKEN_PRESENT;
  path_slot_map_[path] = slot_id;
  LOG(INFO) << "Slot " << slot_id << " ready for token at " << path.value();
  VLOG(1) << "SlotManagerImpl::OnLogin success";
}

void SlotManagerImpl::OnLogout(const FilePath& path) {
  VLOG(1) << "SlotManagerImpl::OnLogout enter";
  // If we're not managing this token, ignore the event.
  if (path_slot_map_.find(path) == path_slot_map_.end()) {
    LOG(WARNING) << "Logout event received for unknown path: " << path.value();
    return;
  }
  int slot_id = path_slot_map_[path];

  // Wait for initialization to be finished before cleaning up.
  if (slot_list_[slot_id].worker_thread.get())
    base::PlatformThread::Join(slot_list_[slot_id].worker_thread_handle);

  // Spawn a thread to handle the TPM-related work.
  slot_list_[slot_id].worker_thread.reset(new TokenTermThread(slot_id,
                                                              tpm_utility_));
  base::PlatformThread::Create(0,
                               slot_list_[slot_id].worker_thread.get(),
                               &slot_list_[slot_id].worker_thread_handle);

  CloseAllSessions(slot_id);
  slot_list_[slot_id].token_object_pool.reset();
  slot_list_[slot_id].slot_info.flags &= ~CKF_TOKEN_PRESENT;
  path_slot_map_.erase(path);
  LOG(INFO) << "Token at " << path.value() << " has been removed from slot "
            << slot_id;
  VLOG(1) << "SlotManagerImpl::OnLogout success";
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
    object_pool = factory_->CreateObjectPool(this,
                                             factory_->CreateObjectStore(path),
                                             NULL);
    scoped_object_pool.reset(object_pool);
    slot_id = FindEmptySlot();
    unload = true;
  } else {
    slot_id = path_slot_map_[path];
    object_pool = slot_list_[slot_id].token_object_pool.get();
  }
  CHECK(object_pool);
  // Before we attempt the change, sanity check old_auth_data.
  string saved_auth_data_hash;
  object_pool->GetInternalBlob(kAuthDataHash, &saved_auth_data_hash);
  if (!SanityCheckAuthData(HashAuthData(old_auth_data), saved_auth_data_hash)) {
    LOG(ERROR) << "Old authorization data is not correct.";
    return;
  }
  string auth_key_blob;
  string new_auth_key_blob;
  if (!object_pool->GetInternalBlob(kEncryptedAuthKey, &auth_key_blob)) {
    LOG(INFO) << "Token not initialized; ignoring change auth data event.";
  } else if (!tpm_utility_->ChangeAuthData(slot_id,
                                           Sha1(old_auth_data),
                                           Sha1(new_auth_data),
                                           auth_key_blob,
                                           &new_auth_key_blob)) {
    LOG(ERROR) << "Failed to change auth data for token at " << path.value();
  } else if (!object_pool->SetInternalBlob(kEncryptedAuthKey,
                                           new_auth_key_blob)) {
    LOG(ERROR) << "Failed to write changed auth blob for token at "
               << path.value();
  } else if (!object_pool->SetInternalBlob(kAuthDataHash,
                                           HashAuthData(new_auth_data))) {
    LOG(ERROR) << "Failed to write auth data hash for token at "
               << path.value();
  }
  if (unload)
    tpm_utility_->UnloadKeysForSlot(slot_id);
}

int SlotManagerImpl::CreateHandle() {
  AutoLock lock(handle_generator_lock_);
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
