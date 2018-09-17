// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_BOUNCER_ENTRY_MANAGER_H_
#define USB_BOUNCER_ENTRY_MANAGER_H_

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/memory/ref_counted.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "usb_bouncer/usb_bouncer.pb.h"

namespace usb_bouncer {

using DevpathToRuleCallback =
    std::function<std::string(const std::string& devpath)>;

constexpr char kDefaultGlobalDir[] = "run/usb_bouncer/";
constexpr char kUsbguardPolicyDir[] = "etc/usbguard/rules.d";

// Keep track of white-list rules needed for trusted USB devices for
// usbguard-daemon. Specifically maintains lists of:
//   1) Rules representing the currently connected devices
//   2) Optionally, Rules for USB devices that were present while the primary
//      user was signed into a session.
//
// In general only one instance of EntryManager should exist at a time. For
// example the default instance can be used to invoke a member function:
//   EntryManager::GetInstance()->GenerateRules();
class EntryManager {
 public:
  enum class UdevAction { kAdd = 0, kRemove = 1 };

  static EntryManager* GetInstance();
  static bool CreateDefaultGlobalDB();

  ~EntryManager();

  // Removes expired entries from the trash of the global DB.
  bool GarbageCollect();

  // Returns a string representation of the contents of a rules.conf file that
  // can be used by usbguard-daemon.
  std::string GenerateRules() const;

  // Updates the internal databases based on the particular |action| for the
  // given |devpath|. Note that |devpath| isn't a valid path unitl "/sys" is
  // prepended to be consistent with udev.
  // For |action|:
  //   kAdd: whitelist entries are added to the global DB and to
  //       the user DB if it is available.
  //   kRemove: whitelist entries in the global DB are moved to the trash map
  //       incase a device uses mode switching, so each mode can be added to a
  //       single entry in the database.
  bool HandleUdev(UdevAction action, const std::string& devpath);

  // Updates entries in the user DB with all entries in the global DB. This
  // allows Entries for currently connected devices that mode switch to be
  // propagated to the primary user on sign-in or unlock.
  bool HandleUserLogin();

 private:
  friend class EntryManagerTestUtil;

  EntryManager();
  EntryManager(const std::string& root_dir, const base::FilePath& user_db_dir,
               DevpathToRuleCallback rule_from_devpath);

  // Removes expired entries from the trash of the global DB. If |global_only|
  // is false expired entries are removed from the user DB as well. This does
  // not write to disk so PersistChanges() needs to be called afterward. Returns
  // the number of removed entries.
  size_t GarbageCollectInternal(bool global_only);

  // Returns true if "/sys" + devpath expands to a child path of /sys/devices/.
  bool ValidateDevPath(const std::string& devpath);

  bool PersistChanges();

  // Prepended to all the paths to enable testing.
  base::FilePath root_dir_;
  // Allows mocking this functionality for tests.
  DevpathToRuleCallback rule_from_devpath_;

  base::FilePath global_db_path_;
  std::unique_ptr<RuleDB> global_entries_;

  base::FilePath user_db_path_;
  std::unique_ptr<RuleDB> user_entries_;
};

}  // namespace usb_bouncer

#endif  // USB_BOUNCER_ENTRY_MANAGER_H_
