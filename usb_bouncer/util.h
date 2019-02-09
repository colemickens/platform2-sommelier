// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USB_BOUNCER_UTIL_H_
#define USB_BOUNCER_UTIL_H_

#include <base/files/file_path.h>
#include <base/time/time.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/timestamp.pb.h>
#include <unistd.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "usb_bouncer/usb_bouncer.pb.h"

namespace usb_bouncer {

using google::protobuf::Timestamp;
using EntryMap = google::protobuf::Map<google::protobuf::string, RuleEntry>;

constexpr char kUsbBouncerUser[] = "usb_bouncer";
constexpr char kUsbBouncerGroup[] = "usb_bouncer";

constexpr char kDefaultDbName[] = "devices.proto";
constexpr char kUserDbParentDir[] = "/run/daemon-store/usb_bouncer";

constexpr uid_t kRootUid = 0;

std::string Hash(const std::string& content);
std::string Hash(const google::protobuf::RepeatedPtrField<std::string>& rules);

// Sets |db_path| to the path of the DB file (it may not exist) and returns a
// RuleDB (creating one if necessary). On failure nullptr is returned.
std::unique_ptr<RuleDB> GetDBFromPath(const base::FilePath& parent_dir,
                                      base::FilePath* db_path);

// Invokes usbguard to get a rule corresponding to |devpath|. Note that
// |devpath| isn't actually a valid path until you prepend "/sys". This matches
// the behavior of udev. The return value is a allow-list rule from usbguard
// with the port specific fields removed.
std::string GetRuleFromDevPath(const std::string& devpath);

// Returns false for rules that should not be included in the allow-list at the
// lock screen. The basic idea is to exclude devices whose function cannot be
// performed if they are first plugged in at the lock screen. Some examples
// include printers, scanners, and USB storage devices.
bool IncludeRuleAtLockscreen(const std::string& rule);

// Returns false if rule is not a valid rule.
bool ValidateRule(const std::string& rule);

// Returns the path where the user DB should be written if there is a user
// signed in and CrOS is unlocked. Otherwise, returns an empty path. In the
// multi-login case, the primary user's daemon-store is used.
base::FilePath GetUserDBDir();

// Returns true if the lock screen is being shown. On a D-Bus failure true is
// returned because that is the safer failure state. This may result in some
// devices not being added to a user's allow-list, but that is safer than a
// malicious device being added to the allow-list while at the lock-screen.
bool IsLockscreenShown();

std::string StripLeadingPathSeparators(const std::string& path);

// Returns a set of all the rules present in |entries|. This serves as a
// filtering step prior to generating the rules configuration for
// usbguard-daemon so that there aren't duplicate rules. The rules are
// de-duplicated by string value ignoring any metadata like the time last used.
std::unordered_set<std::string> UniqueRules(const EntryMap& entries);

bool WriteProtoToPath(const base::FilePath& db_path,
                      google::protobuf::MessageLite* rule_db);

////////////////////////////////////////////////////////////////////////////////
// Time related helper functions.
////////////////////////////////////////////////////////////////////////////////
void UpdateTimestamp(Timestamp* timestamp);
size_t RemoveEntriesOlderThan(base::TimeDelta cutoff, EntryMap* map);

}  // namespace usb_bouncer

#endif  // USB_BOUNCER_UTIL_H_
