// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usb_bouncer/util.h"

#include <base/base64.h>
#include <base/files/file_util.h>
#include <base/process/launch.h>
#include <base/strings/string_piece.h>
#include <base/strings/string_util.h>
#include <brillo/cryptohome.h>
#include <brillo/file_utils.h>
#include <brillo/userdb_utils.h>
#include <openssl/sha.h>
#include <session_manager/dbus-proxies.h>
#include <unistd.h>

#include <cstring>
#include <vector>

#include <usbguard/Device.hpp>
#include <usbguard/DeviceManager.hpp>
#include <usbguard/DeviceManagerHooks.hpp>
#include <usbguard/Rule.hpp>

using brillo::cryptohome::home::GetHashedUserPath;
using org::chromium::SessionManagerInterfaceProxy;

namespace usb_bouncer {

namespace {

constexpr int kDbPermissions =
    base::FILE_PERMISSION_READ_BY_USER | base::FILE_PERMISSION_WRITE_BY_USER;

// Returns base64 encoded strings since proto strings must be valid UTF-8.
std::string EncodeDigest(const std::vector<uint8_t>& digest) {
  std::string result;
  base::StringPiece digest_view(reinterpret_cast<const char*>(digest.data()),
                                digest.size());
  base::Base64Encode(digest_view, &result);
  return result;
}

std::unique_ptr<SessionManagerInterfaceProxy> SetUpDBus(
    scoped_refptr<dbus::Bus> bus) {
  if (!bus) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;

    bus = new dbus::Bus(options);
    CHECK(bus->Connect());
  }
  return std::make_unique<SessionManagerInterfaceProxy>(bus);
}

class UsbguardDeviceManagerHooksImpl : public usbguard::DeviceManagerHooks {
 public:
  void dmHookDeviceEvent(usbguard::DeviceManager::EventType event,
                         std::shared_ptr<usbguard::Device> device) override {
    lastRule_ = *device->getDeviceRule(false /*include_port*/,
                                       false /*with_parent_hash*/);

    // If usbguard-daemon is running when a device is connected, it might have
    // blocked the particular device in which case this will be a block rule.
    // For the purpose of whitelisting, this needs to be an Allow rule.
    lastRule_.setTarget(usbguard::Rule::Target::Allow);
  }

  uint32_t dmHookAssignID() override {
    static uint32_t id = 0;
    return id++;
  }

  void dmHookDeviceException(const std::string& message) override {
    LOG(ERROR) << message;
  }

  std::string getLastRule() { return lastRule_.toString(); }

 private:
  usbguard::Rule lastRule_;
};

// As root this will create the necessary files with the required permissions,
// without root it will try to create the files and verify the permissions are
// correct.
bool SetupPermissionsFor(const base::FilePath& path) {
  uid_t proc_uid = getuid();
  uid_t uid = proc_uid;
  gid_t gid = getgid();

  if (uid == kRootUid &&
      !brillo::userdb::GetUserInfo(kUsbBouncerUser, &uid, &gid)) {
    LOG(ERROR) << "Failed to get uid & gid for \"" << kUsbBouncerUser << "\"";
    return false;
  }
  // TODO(chromium:896337) Address TOCTOU here.
  if (!brillo::TouchFile(path, kDbPermissions, uid, gid)) {
    LOG(ERROR) << "Failed to touch file \"" << path.value() << "\"";
    return false;
  }
  if (proc_uid == kRootUid) {
    base::FilePath parent_dir = path.DirName();
    if (chown(parent_dir.value().c_str(), uid, gid) < 0) {
      PLOG(ERROR) << "chown for \"" << parent_dir.value()
                  << "\" failed because: " << std::strerror(errno);
      return false;
    }
    if (chown(path.value().c_str(), uid, gid) < 0) {
      PLOG(ERROR) << "chown for \"" << path.value()
                  << "\" failed because: " << std::strerror(errno);
      return false;
    }
  }
  if (!base::VerifyPathControlledByUser(path, path, uid, {gid})) {
    LOG(ERROR) << "Wrong permissions \"" << path.value() << "\"";
  }
  return true;
}

}  // namespace

std::string Hash(const std::string& content) {
  std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH, 0);

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, content.data(), content.size());
  SHA256_Final(digest.data(), &ctx);
  return EncodeDigest(digest);
}

std::string Hash(const google::protobuf::RepeatedPtrField<std::string>& rules) {
  std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH, 0);

  SHA256_CTX ctx;
  SHA256_Init(&ctx);

  // This extra logic is needed for consistency with
  // Hash(const std::string& content)
  bool first = true;
  for (const auto& rule : rules) {
    SHA256_Update(&ctx, rule.data(), rule.size());
    if (!first) {
      // Add a end of line to delimit rules for the mode switching case when
      // more than one white-listing rule is needed for a single device.
      SHA256_Update(&ctx, "\n", 1);
    } else {
      first = false;
    }
  }

  SHA256_Final(digest.data(), &ctx);
  return EncodeDigest(digest);
}

std::unique_ptr<RuleDB> GetDBFromPath(const base::FilePath& parent_dir,
                                      base::FilePath* db_path) {
  *db_path = parent_dir.Append(kDefaultDbName);
  // TODO(chromium:896337) Fix TOCTOU.
  if (!SetupPermissionsFor(*db_path)) {
    *db_path = base::FilePath("");
    return nullptr;
  }
  std::string data;
  if (!base::ReadFileToString(*db_path, &data) || data.empty()) {
    return std::make_unique<RuleDB>();
  }

  auto result = std::make_unique<RuleDB>();
  if (!result->ParseFromString(data)) {
    LOG(ERROR) << "Error parsing db. Regenerating...";
  }
  return result;
}

std::string GetRuleFromDevPath(const std::string& devpath) {
  UsbguardDeviceManagerHooksImpl hooks;
  auto device_manager = usbguard::DeviceManager::create(hooks, "uevent");
  device_manager->setEnumerationOnlyMode(true);
  device_manager->scan(devpath);
  return hooks.getLastRule();
}

bool ValidateRule(const std::string& rule) {
  if (rule.empty()) {
    return false;
  }
  return usbguard::Rule::fromString(rule);
}

base::FilePath GetUserDBDir() {
  scoped_refptr<dbus::Bus> bus;
  auto session_manager_proxy = SetUpDBus(bus);

  brillo::ErrorPtr error;
  std::string username, hashed_username;
  session_manager_proxy->RetrievePrimarySession(&username, &hashed_username,
                                                &error);
  if (hashed_username.empty()) {
    LOG(ERROR) << "No active user session.";
    return base::FilePath("");
  }

  bool locked;
  if (!session_manager_proxy->IsScreenLocked(&locked, &error)) {
    LOG(ERROR) << "Failed to get lockscreen state.";
    return base::FilePath("");
  }
  if (locked) {
    LOG(INFO) << "Device is locked.";
    return base::FilePath("");
  }

  base::FilePath UserDir =
      base::FilePath(kUserDbParentDir).Append(hashed_username);
  if (!base::DirectoryExists(UserDir)) {
    LOG(ERROR) << "User daemon-store directory doesn't exist.";
    return base::FilePath("");
  }

  return UserDir;
}

std::string StripLeadingPathSeparators(const std::string& path) {
  return path.substr(path.find_first_not_of('/'));
}

std::unordered_set<std::string> UniqueRules(const EntryMap& entries) {
  std::unordered_set<std::string> aggregated_rules;
  for (const auto& entry_itr : entries) {
    for (const auto& rule : entry_itr.second.rules()) {
      if (!rule.empty()) {
        aggregated_rules.insert(rule);
      }
    }
  }
  return aggregated_rules;
}

bool WriteProtoToPath(const base::FilePath& db_path,
                      google::protobuf::MessageLite* rule_db) {
  std::string serialized = rule_db->SerializeAsString();
  int result = base::WriteFile(db_path, serialized.data(), serialized.size());
  if (result != serialized.size()) {
    LOG(ERROR) << "Failed to write proto to file!";
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Time related helper functions.
////////////////////////////////////////////////////////////////////////////////

void UpdateTimestamp(Timestamp* timestamp) {
  auto time = (base::Time::Now() - base::Time::UnixEpoch()).ToTimeSpec();
  timestamp->set_seconds(time.tv_sec);
  timestamp->set_nanos(time.tv_nsec);
}

size_t RemoveEntriesOlderThan(base::TimeDelta cutoff, EntryMap* map) {
  size_t num_removed = 0;
  auto itr = map->begin();
  auto cuttoff_time =
      (base::Time::Now() - base::Time::UnixEpoch() - cutoff).ToTimeSpec();
  while (itr != map->end()) {
    const Timestamp& entry_timestamp = itr->second.last_used();
    if (entry_timestamp.seconds() < cuttoff_time.tv_sec ||
        (entry_timestamp.seconds() == cuttoff_time.tv_sec &&
         entry_timestamp.nanos() < cuttoff_time.tv_nsec)) {
      ++num_removed;
      map->erase(itr++);
    } else {
      ++itr;
    }
  }
  return num_removed;
}

}  // namespace usb_bouncer
