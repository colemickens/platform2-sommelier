// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cryptohome client that uses the dbus client interface

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/utility.h>

#include <iostream>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <termios.h>
#include <unistd.h>

#include "cryptohome/username_passkey.h"
#include "cryptohome/bindings/client.h"

namespace switches {
  static const char kActionSwitch[] = "action";
  static const char *kActions[] = {
    "mount",
    "unmount",
    "is_mounted",
    "test_auth",
    "migrate_key",
    "remove",
    NULL };
  enum ActionEnum {
    ACTION_MOUNT,
    ACTION_UNMOUNT,
    ACTION_MOUNTED,
    ACTION_TEST_AUTH,
    ACTION_MIGRATE_KEY,
    ACTION_REMOVE };
  static const char kUserSwitch[] = "user";
  static const char kPasswordSwitch[] = "password";
  static const char kOldPasswordSwitch[] = "old_password";
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

bool GetUsernamePassword(const chromeos::dbus::Proxy& proxy,
                         const CommandLine* cl,
                         std::string* user_out,
                         std::string* password_out,
                         std::string* old_password_out = NULL) {
  if(!GetUsername(cl, user_out)) {
    return false;
  }

  std::string password = cl->GetSwitchValueASCII(switches::kPasswordSwitch);
  std::string old_password = cl->GetSwitchValueASCII(
      switches::kOldPasswordSwitch);

  if(old_password_out != NULL && old_password.length() == 0) {
    struct termios original_attr;
    struct termios new_attr;
    tcgetattr(0, &original_attr);
    memcpy(&new_attr, &original_attr, sizeof(new_attr));
    new_attr.c_lflag &= ~(ECHO);
    tcsetattr(0, TCSANOW, &new_attr);
    std::cout << "Enter old password for <" << (*user_out) << ">: ";
    std::cin >> old_password;
    std::cout << std::endl;
    tcsetattr(0, TCSANOW, &original_attr);
  }
  if(password.length() == 0) {
    struct termios original_attr;
    struct termios new_attr;
    tcgetattr(0, &original_attr);
    memcpy(&new_attr, &original_attr, sizeof(new_attr));
    new_attr.c_lflag &= ~(ECHO);
    tcsetattr(0, TCSANOW, &new_attr);
    std::cout << "Enter password for <" << (*user_out) << ">: ";
    std::cin >> password;
    std::cout << std::endl
              << "Re-enter password for <" << (*user_out) << ">: ";
    std::string verification;
    std::cin >> verification;
    std::cout << std::endl;
    tcsetattr(0, TCSANOW, &original_attr);
    if(verification != password) {
      LOG(ERROR) << "Passwords do not match.";
      return false;
    }
  }

  std::string trimmed_password;
  TrimString(password, "\r\n", &trimmed_password);
  cryptohome::UsernamePasskey up(
      cryptohome::UsernamePasskey::FromUsernamePassword((*user_out).c_str(),
          trimmed_password.c_str(), GetSystemSalt(proxy)));
  cryptohome::SecureBlob passkey = up.GetPasskey();
  *password_out = std::string(reinterpret_cast<char*>(&passkey[0]),
                              passkey.size());
  if(old_password_out != NULL) {
    TrimString(old_password, "\r\n", &trimmed_password);
    cryptohome::UsernamePasskey old_up(
        cryptohome::UsernamePasskey::FromUsernamePassword((*user_out).c_str(),
            trimmed_password.c_str(), GetSystemSalt(proxy)));
    passkey = old_up.GetPasskey();
    *old_password_out = std::string(reinterpret_cast<char*>(&passkey[0]),
                                    passkey.size());
  }

  return true;
}

bool AskHardQuestion(const std::string& user) {
  std::cout << "!!! The password you entered did not unwrap the user's"
            << std::endl
            << "!!! vault key.  If you proceed, the user's cryptohome"
            << std::endl
            << "!!! will be DELETED and RECREATED."
            << std::endl
            << "!!!"
            << std::endl
            << "!!! Re-enter the username at the prompt to create a new"
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

    if(!GetUsernamePassword(proxy, cl, &user, &password)) {
      return 1;
    }

    gboolean done = false;
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    org_chromium_CryptohomeInterface_check_key(proxy.gproxy(),
                                               user.c_str(),
                                               password.c_str(),
                                               &done,
                                               errptr);
    if(!done) {
      // TODO(fes): Remove when Mount no longer automatically deletes cryptohome
      // on passkey failure.
      if(!AskHardQuestion(user)) {
        return 1;
      }
    }

    errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_mount(proxy.gproxy(),
                                                user.c_str(),
                                                password.c_str(),
                                                &done,
                                                errptr)) {
      LOG(ERROR) << "Mount call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "Mount did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_TEST_AUTH],
                     action.c_str())) {
    std::string user, password;

    if(!GetUsernamePassword(proxy, cl, &user, &password)) {
      return 1;
    }

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

    if(!GetUsernamePassword(proxy, cl, &user, &password, &old_password)) {
      return 1;
    }

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

    if(!ConfirmRemove(user)) {
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
