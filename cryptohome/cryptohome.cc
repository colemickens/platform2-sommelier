// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cryptohome client that uses the dbus client interface

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <termios.h>
#include <unistd.h>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/utility.h>
#include <iostream>

#include "bindings/client.h"
#include "crypto.h"
#include "secure_blob.h"
#include "tpm.h"
#include "username_passkey.h"


namespace switches {
  static const char kActionSwitch[] = "action";
  static const char *kActions[] = {
    "mount",
    "mount_guest",
    "unmount",
    "is_mounted",
    "test_auth",
    "migrate_key",
    "remove",
    "obfuscate_user",
    NULL };
  enum ActionEnum {
    ACTION_MOUNT,
    ACTION_MOUNT_GUEST,
    ACTION_UNMOUNT,
    ACTION_MOUNTED,
    ACTION_TEST_AUTH,
    ACTION_MIGRATE_KEY,
    ACTION_REMOVE,
    ACTION_OBFUSCATE_USER };
  static const char kUserSwitch[] = "user";
  static const char kPasswordSwitch[] = "password";
  static const char kOldPasswordSwitch[] = "old_password";
  static const char kForceSwitch[] = "force";
}  // namespace switches

chromeos::Blob GetSystemSalt(const chromeos::dbus::Proxy& proxy) {
  chromeos::glib::ScopedError error;
  GArray* salt;
  GError **errptr = &chromeos::Resetter(&error).lvalue();
  if (!org_chromium_CryptohomeInterface_get_system_salt(proxy.gproxy(),
                                                        &salt,
                                                        errptr)) {
    LOG(ERROR) << "GetSystemSalt failed: " << error->message;
    return chromeos::Blob();
  }

  chromeos::Blob system_salt;
  system_salt.resize(salt->len);
  if(system_salt.size() == salt->len) {
    memcpy(&system_salt[0], static_cast<const void*>(salt->data), salt->len);
  } else {
    system_salt.clear();
  }
  g_array_free(salt, false);
  return system_salt;
}

bool GetUsername(const CommandLine* cl, std::string* user_out) {
  *user_out = cl->GetSwitchValueASCII(switches::kUserSwitch);

  if(user_out->length() == 0) {
    LOG(ERROR) << "No user specified (--user=<user>)";
    return false;
  }
  return true;
}

bool GetPassword(const chromeos::dbus::Proxy& proxy,
                 const CommandLine* cl,
                 const std::string& cl_switch,
                 const std::string& prompt,
                 std::string* password_out) {
  std::string password = cl->GetSwitchValueASCII(cl_switch);

  if(password.length() == 0) {
    struct termios original_attr;
    struct termios new_attr;
    tcgetattr(0, &original_attr);
    memcpy(&new_attr, &original_attr, sizeof(new_attr));
    new_attr.c_lflag &= ~(ECHO);
    tcsetattr(0, TCSANOW, &new_attr);
    std::cout << prompt << ": ";
    std::cin >> password;
    std::cout << std::endl;
    tcsetattr(0, TCSANOW, &original_attr);
  }

  std::string trimmed_password;
  TrimString(password, "\r\n", &trimmed_password);
  cryptohome::SecureBlob passkey;
  cryptohome::Crypto::PasswordToPasskey(trimmed_password.c_str(),
                                        GetSystemSalt(proxy), &passkey);
  *password_out = std::string(static_cast<char*>(passkey.data()),
                              passkey.size());

  return true;
}

bool ConfirmRemove(const std::string& user) {
  std::cout << "!!! Are you sure you want to remove the user's cryptohome?"
            << std::endl
            << "!!!"
            << std::endl
            << "!!! Re-enter the username at the prompt to remove the"
            << std::endl
            << "!!! cryptohome for the user."
            << std::endl
            << "Enter the username <" << user << ">: ";
  std::string verification;
  std::cin >> verification;
  if(user != verification) {
    LOG(ERROR) << "Usernames do not match.";
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);

  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string action = cl->GetSwitchValueASCII(switches::kActionSwitch);
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              cryptohome::kCryptohomeServiceName,
                              cryptohome::kCryptohomeServicePath,
                              cryptohome::kCryptohomeInterface);
  DCHECK(proxy.gproxy()) << "Failed to acquire proxy";

  if (!strcmp(switches::kActions[switches::ACTION_MOUNT], action.c_str())) {
    std::string user, password;

    if (!GetUsername(cl, &user)) {
      LOG(ERROR) << "No username specified";
      return 1;
    }

    GetPassword(proxy, cl, switches::kPasswordSwitch,
                StringPrintf("Enter the password for <%s>", user.c_str()),
                &password);

    gboolean done = false;
    gint mount_error = 0;
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_mount(proxy.gproxy(),
                                                user.c_str(),
                                                password.c_str(),
                                                &mount_error,
                                                &done,
                                                errptr)) {
      LOG(ERROR) << "Mount call failed: " << error->message
                 << ", with reason code: " << mount_error;
    }
    LOG_IF(ERROR, !done) << "Mount did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_MOUNT_GUEST],
                action.c_str())) {
    gboolean done = false;
    gint mount_error = 0;
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_mount_guest(proxy.gproxy(),
                                                      &mount_error,
                                                      &done,
                                                      errptr)) {
      LOG(ERROR) << "MountGuest call failed: " << error->message
                 << ", with reason code: " << mount_error;
    }
    LOG_IF(ERROR, !done) << "MountGuest did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_TEST_AUTH],
                     action.c_str())) {
    std::string user, password;

    if (!GetUsername(cl, &user)) {
      LOG(ERROR) << "No username specified";
      return 1;
    }

    GetPassword(proxy, cl, switches::kPasswordSwitch,
                StringPrintf("Enter the password for <%s>", user.c_str()),
                &password);

    gboolean done = false;
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_check_key(proxy.gproxy(),
                                                    user.c_str(),
                                                    password.c_str(),
                                                    &done,
                                                    errptr)) {
      LOG(ERROR) << "CheckKey call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "CheckKey did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_MIGRATE_KEY],
                     action.c_str())) {
    std::string user, password, old_password;

    if (!GetUsername(cl, &user)) {
      LOG(ERROR) << "No username specified";
      return 1;
    }

    GetPassword(proxy, cl, switches::kPasswordSwitch,
                StringPrintf("Enter the password for <%s>", user.c_str()),
                &password);
    GetPassword(proxy, cl, switches::kOldPasswordSwitch,
                StringPrintf("Enter the old password for <%s>", user.c_str()),
                &old_password);

    gboolean done = false;
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_migrate_key(proxy.gproxy(),
                                                      user.c_str(),
                                                      old_password.c_str(),
                                                      password.c_str(),
                                                      &done,
                                                      errptr)) {
      LOG(ERROR) << "MigrateKey call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "MigrateKey did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_REMOVE],
                     action.c_str())) {
    std::string user;

    if(!GetUsername(cl, &user)) {
      return 1;
    }

    if(!cl->HasSwitch(switches::kForceSwitch) && !ConfirmRemove(user)) {
      return 1;
    }

    gboolean done = false;
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_remove(proxy.gproxy(),
                                                 user.c_str(),
                                                 &done,
                                                 errptr)) {
      LOG(ERROR) << "Remove call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "Remove did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_UNMOUNT],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    gboolean done = false;
    if (!org_chromium_CryptohomeInterface_unmount(proxy.gproxy(),
                                                  &done,
                                                  errptr)) {
      LOG(ERROR) << "Unmount call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "Unmount did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_MOUNTED],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    gboolean done = false;
    if (!org_chromium_CryptohomeInterface_is_mounted(proxy.gproxy(),
                                                     &done,
                                                     errptr)) {
      LOG(ERROR) << "IsMounted call failed: " << error->message;
    }
    std::cout << done << std::endl;

  } else if (!strcmp(switches::kActions[switches::ACTION_OBFUSCATE_USER],
                     action.c_str())) {
    std::string user;

    if(!GetUsername(cl, &user)) {
      return 1;
    }

    cryptohome::UsernamePasskey up(user.c_str(), cryptohome::SecureBlob());
    LOG(INFO) << up.GetObfuscatedUsername(GetSystemSalt(proxy));
  } else {
    LOG(ERROR) << "Unknown action or no action given.  Available actions: ";
    for(int i = 0; /* loop forever */; i++) {
      if(!switches::kActions[i]) {
        break;
      }
      LOG(ERROR) << "  --action=" << switches::kActions[i];
    }
  }
  return 0;
}
