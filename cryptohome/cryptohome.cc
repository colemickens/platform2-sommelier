// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cryptohome client that uses the dbus client interface

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
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
#include <base/stringprintf.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/syslog_logging.h>
#include <chromeos/utility.h>

#include "bindings/client.h"
#include "marshal.glibmarshal.h"
#include "crypto.h"
#include "mount.h"
#include "pkcs11_init.h"
#include "platform.h"
#include "secure_blob.h"
#include "tpm.h"
#include "username_passkey.h"
#include "vault_keyset.pb.h"

using std::string;

namespace switches {
  static const char kSyslogSwitch[] = "syslog";
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
    "dump_keyset",
    "tpm_status",
    "status",
    "set_current_user_old",
    "do_free_disk_space_control",
    "tpm_take_ownership",
    "tpm_clear_stored_password",
    "tpm_wait_ownership",
    "install_attributes_set",
    "install_attributes_get",
    "install_attributes_finalize",
    "pkcs11_init",
    "force_pkcs11_init",
    "pkcs11_token_status",
    "pkcs11_terminate",
    NULL };
  enum ActionEnum {
    ACTION_MOUNT,
    ACTION_MOUNT_GUEST,
    ACTION_UNMOUNT,
    ACTION_MOUNTED,
    ACTION_TEST_AUTH,
    ACTION_MIGRATE_KEY,
    ACTION_REMOVE,
    ACTION_OBFUSCATE_USER,
    ACTION_DUMP_KEYSET,
    ACTION_TPM_STATUS,
    ACTION_STATUS,
    ACTION_SET_CURRENT_USER_OLD,
    ACTION_DO_FREE_DISK_SPACE_CONTROL,
    ACTION_TPM_TAKE_OWNERSHIP,
    ACTION_TPM_CLEAR_STORED_PASSWORD,
    ACTION_TPM_WAIT_OWNERSHIP,
    ACTION_INSTALL_ATTRIBUTES_SET,
    ACTION_INSTALL_ATTRIBUTES_GET,
    ACTION_INSTALL_ATTRIBUTES_FINALIZE,
    ACTION_PKCS11_INIT,
    ACTION_FORCE_PKCS11_INIT,
    ACTION_PKCS11_TOKEN_STATUS,
    ACTION_PKCS11_TERMINATE };
  static const char kUserSwitch[] = "user";
  static const char kPasswordSwitch[] = "password";
  static const char kOldPasswordSwitch[] = "old_password";
  static const char kForceSwitch[] = "force";
  static const char kAsyncSwitch[] = "async";
  static const char kCreateSwitch[] = "create";
  static const char kAttrNameSwitch[] = "name";
  static const char kAttrValueSwitch[] = "value";
}  // namespace switches

chromeos::Blob GetSystemSalt(const chromeos::dbus::Proxy& proxy) {
  chromeos::glib::ScopedError error;
  GArray* salt;
  if (!org_chromium_CryptohomeInterface_get_system_salt(proxy.gproxy(),
      &salt,
      &chromeos::Resetter(&error).lvalue())) {
    LOG(ERROR) << "GetSystemSalt failed: " << error->message;
    return chromeos::Blob();
  }

  chromeos::Blob system_salt;
  system_salt.resize(salt->len);
  if (system_salt.size() == salt->len) {
    memcpy(&system_salt[0], static_cast<const void*>(salt->data), salt->len);
  } else {
    system_salt.clear();
  }
  g_array_free(salt, false);
  return system_salt;
}

bool GetAttrName(const CommandLine* cl, std::string* name_out) {
  *name_out = cl->GetSwitchValueASCII(switches::kAttrNameSwitch);

  if (name_out->length() == 0) {
    printf("No install attribute name specified (--name=<name>)\n");
    return false;
  }
  return true;
}

bool GetAttrValue(const CommandLine* cl, std::string* value_out) {
  *value_out = cl->GetSwitchValueASCII(switches::kAttrValueSwitch);

  if (value_out->length() == 0) {
    printf("No install attribute value specified (--value=<value>)\n");
    return false;
  }
  return true;
}

bool GetUsername(const CommandLine* cl, std::string* user_out) {
  *user_out = cl->GetSwitchValueASCII(switches::kUserSwitch);

  if (user_out->length() == 0) {
    printf("No user specified (--user=<user>)\n");
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

  if (password.length() == 0) {
    struct termios original_attr;
    struct termios new_attr;
    tcgetattr(0, &original_attr);
    memcpy(&new_attr, &original_attr, sizeof(new_attr));
    new_attr.c_lflag &= ~(ECHO);
    tcsetattr(0, TCSANOW, &new_attr);
    printf("%s: ", prompt.c_str());
    fflush(stdout);
    std::cin >> password;
    printf("\n");
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
  printf("!!! Are you sure you want to remove the user's cryptohome?\n");
  printf("!!!\n");
  printf("!!! Re-enter the username at the prompt to remove the\n");
  printf("!!! cryptohome for the user.\n");
  printf("Enter the username <%s>: ", user.c_str());
  fflush(stdout);
  std::string verification;
  std::cin >> verification;
  if (user != verification) {
    printf("Usernames do not match.\n");
    return false;
  }
  return true;
}

class ClientLoop {
 public:
  ClientLoop()
      : loop_(NULL),
        async_call_id_(0),
        return_status_(false),
        return_code_(0) { }

  virtual ~ClientLoop() {
    if (loop_) {
      g_main_loop_unref(loop_);
    }
  }

  void Initialize(chromeos::dbus::Proxy* proxy) {
    dbus_g_object_register_marshaller(cryptohome_VOID__INT_BOOLEAN_INT,
                                      G_TYPE_NONE,
                                      G_TYPE_INT,
                                      G_TYPE_BOOLEAN,
                                      G_TYPE_INT,
                                      G_TYPE_INVALID);
    dbus_g_proxy_add_signal(proxy->gproxy(), "AsyncCallStatus",
                            G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_INT,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy->gproxy(), "AsyncCallStatus",
                                G_CALLBACK(ClientLoop::CallbackThunk),
                                this, NULL);
    loop_ = g_main_loop_new(NULL, TRUE);
  }

  void Run(int async_call_id) {
    async_call_id_ = async_call_id;
    g_main_loop_run(loop_);
  }

  bool get_return_status() {
    return return_status_;
  }

  int get_return_code() {
    return return_code_;
  }

 private:
  void Callback(int async_call_id, bool return_status, int return_code) {
    if (async_call_id == async_call_id_) {
      return_status_ = return_status;
      return_code_ = return_code;
      g_main_loop_quit(loop_);
    }
  }

  static void CallbackThunk(DBusGProxy* proxy,
                            int async_call_id, bool return_status,
                            int return_code, gpointer userdata) {
    reinterpret_cast<ClientLoop*>(userdata)->Callback(async_call_id,
                                                      return_status,
                                                      return_code);
  }

  GMainLoop *loop_;
  int async_call_id_;
  bool return_status_;
  int return_code_;
};

class TpmWaitLoop {
 public:
  TpmWaitLoop()
      : loop_(NULL) { }

  virtual ~TpmWaitLoop() {
    if (loop_) {
      g_main_loop_unref(loop_);
    }
  }

  void Initialize(chromeos::dbus::Proxy* proxy) {
    dbus_g_object_register_marshaller(cryptohome_VOID__BOOLEAN_BOOLEAN_BOOLEAN,
                                      G_TYPE_NONE,
                                      G_TYPE_BOOLEAN,
                                      G_TYPE_BOOLEAN,
                                      G_TYPE_BOOLEAN,
                                      G_TYPE_INVALID);
    dbus_g_proxy_add_signal(proxy->gproxy(), "TpmInitStatus",
                            G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(proxy->gproxy(), "TpmInitStatus",
                                G_CALLBACK(TpmWaitLoop::CallbackThunk),
                                this, NULL);
    loop_ = g_main_loop_new(NULL, TRUE);
  }

  void Run() {
    g_main_loop_run(loop_);
  }

 private:
  static void CallbackThunk(DBusGProxy* proxy, bool ready, bool is_owned,
                            bool took_ownership, gpointer userdata) {
    printf("TPM ready: %s\n", (ready ? "true" : "false"));
    printf("TPM owned: %s\n", (is_owned ? "true" : "false"));
    printf("TPM took_ownership: %s\n", (took_ownership ? "true" : "false"));
    g_main_loop_quit(reinterpret_cast<TpmWaitLoop*>(userdata)->loop_);
  }

  GMainLoop *loop_;
};

bool WaitForTPMOwnership(chromeos::dbus::Proxy* proxy) {
  TpmWaitLoop client_loop;
  client_loop.Initialize(proxy);
  gboolean result;
  chromeos::glib::ScopedError error;
  if (!org_chromium_CryptohomeInterface_tpm_is_being_owned(
          proxy->gproxy(), &result, &chromeos::Resetter(&error).lvalue())) {
    printf("TpmIsBeingOwned call failed: %s.\n", error->message);
  }
  if (result) {
    printf("Waiting for TPM to be owned...\n");
    client_loop.Run();
  } else {
    printf("TPM is not currently being owned.\n");
  }
  return result;
}


int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kSyslogSwitch))
    chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  else
    chromeos::InitLog(chromeos::kLogToStderr);

  std::string action = cl->GetSwitchValueASCII(switches::kActionSwitch);
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              cryptohome::kCryptohomeServiceName,
                              cryptohome::kCryptohomeServicePath,
                              cryptohome::kCryptohomeInterface);
  DCHECK(proxy.gproxy()) << "Failed to acquire proxy";
  cryptohome::Platform platform;

  if (!strcmp(switches::kActions[switches::ACTION_MOUNT], action.c_str())) {
    std::string user, password;

    if (!GetUsername(cl, &user)) {
      printf("No username specified.\n");
      return 1;
    }

    GetPassword(proxy, cl, switches::kPasswordSwitch,
                StringPrintf("Enter the password for <%s>", user.c_str()),
                &password);

    gboolean done = false;
    gint mount_error = 0;
    chromeos::glib::ScopedError error;

    if (!cl->HasSwitch(switches::kAsyncSwitch)) {
      if (!org_chromium_CryptohomeInterface_mount(proxy.gproxy(),
               user.c_str(),
               password.c_str(),
               cl->HasSwitch(switches::kCreateSwitch),
               false,
               NULL,
               &mount_error,
               &done,
               &chromeos::Resetter(&error).lvalue())) {
        printf("Mount call failed: %s, with reason code: %d.\n", error->message,
               mount_error);
      }
    } else {
      ClientLoop client_loop;
      client_loop.Initialize(&proxy);
      gint async_id = -1;
      if (!org_chromium_CryptohomeInterface_async_mount(proxy.gproxy(),
               user.c_str(),
               password.c_str(),
               cl->HasSwitch(switches::kCreateSwitch),
               false,
               NULL,
               &async_id,
               &chromeos::Resetter(&error).lvalue())) {
        printf("Mount call failed: %s.\n", error->message);
      } else {
        client_loop.Run(async_id);
        done = client_loop.get_return_status();
      }
    }
    if (!done) {
      printf("Mount failed.\n");
    } else {
      printf("Mount succeeded.\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_MOUNT_GUEST],
                action.c_str())) {
    gboolean done = false;
    gint mount_error = 0;
    chromeos::glib::ScopedError error;

    if (!cl->HasSwitch(switches::kAsyncSwitch)) {
      if (!org_chromium_CryptohomeInterface_mount_guest(proxy.gproxy(),
               &mount_error,
               &done,
               &chromeos::Resetter(&error).lvalue())) {
        printf("MountGuest call failed: %s, with reason code: %d.\n",
               error->message, mount_error);
      }
    } else {
      ClientLoop client_loop;
      client_loop.Initialize(&proxy);
      gint async_id = -1;
      if (!org_chromium_CryptohomeInterface_async_mount_guest(proxy.gproxy(),
               &async_id,
               &chromeos::Resetter(&error).lvalue())) {
        printf("Mount call failed: %s.\n", error->message);
      } else {
        client_loop.Run(async_id);
        done = client_loop.get_return_status();
      }
    }
    if (!done) {
      printf("Mount failed.\n");
    } else {
      printf("Mount succeeded.\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_TEST_AUTH],
                     action.c_str())) {
    std::string user, password;

    if (!GetUsername(cl, &user)) {
      printf("No username specified.\n");
      return 1;
    }

    GetPassword(proxy, cl, switches::kPasswordSwitch,
                StringPrintf("Enter the password for <%s>", user.c_str()),
                &password);

    gboolean done = false;
    chromeos::glib::ScopedError error;

    if (!cl->HasSwitch(switches::kAsyncSwitch)) {
      if (!org_chromium_CryptohomeInterface_check_key(proxy.gproxy(),
               user.c_str(),
               password.c_str(),
               &done,
               &chromeos::Resetter(&error).lvalue())) {
        printf("CheckKey call failed: %s.\n", error->message);
      }
    } else {
      ClientLoop client_loop;
      client_loop.Initialize(&proxy);
      gint async_id = -1;
      if (!org_chromium_CryptohomeInterface_async_check_key(proxy.gproxy(),
               user.c_str(),
               password.c_str(),
               &async_id,
               &chromeos::Resetter(&error).lvalue())) {
        printf("CheckKey call failed: %s.\n", error->message);
      } else {
        client_loop.Run(async_id);
        done = client_loop.get_return_status();
      }
    }
    if (!done) {
      printf("Authentication failed.\n");
    } else {
      printf("Authentication succeeded.\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_MIGRATE_KEY],
                     action.c_str())) {
    std::string user, password, old_password;

    if (!GetUsername(cl, &user)) {
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

    if (!cl->HasSwitch(switches::kAsyncSwitch)) {
      if (!org_chromium_CryptohomeInterface_migrate_key(proxy.gproxy(),
               user.c_str(),
               old_password.c_str(),
               password.c_str(),
               &done,
               &chromeos::Resetter(&error).lvalue())) {
        printf("MigrateKey call failed: %s.\n", error->message);
      }
    } else {
      ClientLoop client_loop;
      client_loop.Initialize(&proxy);
      gint async_id = -1;
      if (!org_chromium_CryptohomeInterface_async_migrate_key(proxy.gproxy(),
               user.c_str(),
               old_password.c_str(),
               password.c_str(),
               &async_id,
               &chromeos::Resetter(&error).lvalue())) {
        printf("MigrateKey call failed: %s.\n", error->message);
      } else {
        client_loop.Run(async_id);
        done = client_loop.get_return_status();
      }
    }
    if (!done) {
      printf("Key migration failed.\n");
    } else {
      printf("Key migration succeeded.\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_REMOVE],
                     action.c_str())) {
    std::string user;

    if (!GetUsername(cl, &user)) {
      return 1;
    }

    if (!cl->HasSwitch(switches::kForceSwitch) && !ConfirmRemove(user)) {
      return 1;
    }

    gboolean done = false;
    chromeos::glib::ScopedError error;
    if (!org_chromium_CryptohomeInterface_remove(proxy.gproxy(),
        user.c_str(),
        &done,
        &chromeos::Resetter(&error).lvalue())) {
      printf("Remove call failed: %s.\n", error->message);
    }
    if (!done) {
      printf("Remove failed.\n");
    } else {
      printf("Remove succeeded.\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_UNMOUNT],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    gboolean done = false;
    if (!org_chromium_CryptohomeInterface_unmount(proxy.gproxy(),
        &done,
        &chromeos::Resetter(&error).lvalue())) {
      printf("Unmount call failed: %s.\n", error->message);
    }
    if (!done) {
      printf("Unmount failed.\n");
    } else {
      printf("Unmount succeeded.\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_MOUNTED],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    gboolean done = false;
    if (!org_chromium_CryptohomeInterface_is_mounted(proxy.gproxy(),
        &done,
        &chromeos::Resetter(&error).lvalue())) {
      printf("IsMounted call failed: %s.\n", error->message);
    }
    if (done) {
      printf("true\n");
    } else {
      printf("false\n");
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_OBFUSCATE_USER],
                     action.c_str())) {
    std::string user;

    if (!GetUsername(cl, &user)) {
      return 1;
    }

    cryptohome::UsernamePasskey up(user.c_str(), cryptohome::SecureBlob());
    printf("%s\n", up.GetObfuscatedUsername(GetSystemSalt(proxy)).c_str());
  } else if (!strcmp(switches::kActions[switches::ACTION_DUMP_KEYSET],
                     action.c_str())) {
    std::string user;

    if (!GetUsername(cl, &user)) {
      return 1;
    }

    cryptohome::UsernamePasskey up(user.c_str(), cryptohome::SecureBlob());

    string vault_path = StringPrintf("/home/.shadow/%s/master.0",
        up.GetObfuscatedUsername(GetSystemSalt(proxy)).c_str());

    cryptohome::SecureBlob contents;
    if (!platform.ReadFile(vault_path, &contents)) {
      printf("Couldn't load keyset contents: %s.\n", vault_path.c_str());
      return 1;
    }
    cryptohome::SerializedVaultKeyset serialized;
    if (!serialized.ParseFromArray(
        static_cast<const unsigned char*>(&contents[0]), contents.size())) {
      printf("Couldn't parse keyset contents: %s.\n", vault_path.c_str());
      return 1;
    }
    printf("For keyset: %s\n", vault_path.c_str());
    printf("  Flags:\n");
    if ((serialized.flags() & cryptohome::SerializedVaultKeyset::TPM_WRAPPED)
        && serialized.has_tpm_key()) {
      printf("    TPM_WRAPPED\n");
    }
    if (serialized.flags()
        & cryptohome::SerializedVaultKeyset::SCRYPT_WRAPPED) {
      printf("    SCRYPT_WRAPPED\n");
    }
    cryptohome::SecureBlob blob(serialized.salt().length());
    serialized.salt().copy(static_cast<char*>(blob.data()),
                           serialized.salt().length(), 0);
    printf("  Salt:\n");
    printf("    %s\n", chromeos::AsciiEncode(blob).c_str());
    blob.resize(serialized.wrapped_keyset().length());
    serialized.wrapped_keyset().copy(static_cast<char*>(blob.data()),
                                     serialized.wrapped_keyset().length(), 0);
    printf("  Wrapped (Encrypted) Keyset:\n");
    printf("    %s\n", chromeos::AsciiEncode(blob).c_str());
    if (serialized.has_tpm_key()) {
      blob.resize(serialized.tpm_key().length());
      serialized.tpm_key().copy(static_cast<char*>(blob.data()),
                                serialized.tpm_key().length(), 0);
      printf("  TPM-Bound (Encrypted) Vault Encryption Key:\n");
      printf("    %s\n", chromeos::AsciiEncode(blob).c_str());
    }
    if (serialized.has_tpm_public_key_hash()) {
      blob.resize(serialized.tpm_public_key_hash().length());
      serialized.tpm_public_key_hash().copy(static_cast<char*>(blob.data()),
                                            serialized.tpm_key().length(), 0);
      printf("  TPM Public Key Hash:\n");
      printf("    %s\n", chromeos::AsciiEncode(blob).c_str());
    }
    if (serialized.has_password_rounds()) {
      printf("  Password rounds:\n");
      printf("    %d\n", serialized.password_rounds());
    }
    if (serialized.has_last_activity_timestamp()) {
      const base::Time last_activity =
          base::Time::FromInternalValue(serialized.last_activity_timestamp());
      printf("  Last activity (days ago):\n");
      printf("    %d\n", (base::Time::Now() - last_activity).InDays());
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_TPM_STATUS],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    gboolean result = false;
    if (!org_chromium_CryptohomeInterface_tpm_is_enabled(proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmIsEnabled call failed: %s.\n", error->message);
    } else {
      printf("TPM Enabled: %s\n", (result ? "true" : "false"));
    }
    result = false;
    if (!org_chromium_CryptohomeInterface_tpm_is_owned(proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmIsOwned call failed: %s.\n", error->message);
    } else {
      printf("TPM Owned: %s\n", (result ? "true" : "false"));
    }
    if (!org_chromium_CryptohomeInterface_tpm_is_being_owned(proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmIsBeingOwned call failed: %s.\n", error->message);
    } else {
      printf("TPM Being Owned: %s\n", (result ? "true" : "false"));
    }
    if (!org_chromium_CryptohomeInterface_tpm_is_ready(proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmIsReady call failed: %s.\n", error->message);
    } else {
      printf("TPM Ready: %s\n", (result ? "true" : "false"));
    }
    gchar* password;
    if (!org_chromium_CryptohomeInterface_tpm_get_password(proxy.gproxy(),
        &password,
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmGetPassword call failed: %s.\n", error->message);
    } else {
      printf("TPM Password: %s\n", password);
      g_free(password);
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_STATUS],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    gchar* status;
    if (!org_chromium_CryptohomeInterface_get_status_string(proxy.gproxy(),
        &status,
        &chromeos::Resetter(&error).lvalue())) {
      printf("GetStatusString call failed: %s.\n", error->message);
    } else {
      printf("%s\n", status);
      g_free(status);
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_SET_CURRENT_USER_OLD],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    ClientLoop client_loop;
    client_loop.Initialize(&proxy);
    if (!org_chromium_CryptohomeInterface_update_current_user_activity_timestamp(
            proxy.gproxy(),
            cryptohome::kOldUserLastActivityTime.InSeconds(),
            &chromeos::Resetter(&error).lvalue())) {
      printf("UpdateCurrentUserActivityTimestamp call failed: %s.\n",
             error->message);
    } else {
        printf("Timestamp successfully updated. You may verify it with "
               "--action=dump_keyset --user=...\n");
    }
  } else if (!strcmp(
      switches::kActions[switches::ACTION_DO_FREE_DISK_SPACE_CONTROL],
      action.c_str())) {
    chromeos::glib::ScopedError error;
    ClientLoop client_loop;
    client_loop.Initialize(&proxy);
    gint async_id = -1;
    if (!org_chromium_CryptohomeInterface_async_do_automatic_free_disk_space_control(
            proxy.gproxy(),
            &async_id,
            &chromeos::Resetter(&error).lvalue())) {
      printf("AsyncDoAutomaticFreeDiskSpaceControl call failed: %s.\n",
             error->message);
    } else {
      client_loop.Run(async_id);
      if (client_loop.get_return_status()) {
        printf("Free disk space control completed successfully "
               "and maybe cut away something. Use `df` to check.\n");
      } else {
        printf("Cleanup reported that there was enough free space "
               "(more than %"PRIu64" Kbytes, check with `df`) "
               "so it didn't try to cut anything.\n",
               cryptohome::kMinFreeSpace >> 10);
      }
    }
  } else if (!strcmp(switches::kActions[switches::ACTION_TPM_TAKE_OWNERSHIP],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    if (!org_chromium_CryptohomeInterface_tpm_can_attempt_ownership(
        proxy.gproxy(),
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmCanAttemptOwnership call failed: %s.\n", error->message);
    }
  } else if (!strcmp(
      switches::kActions[switches::ACTION_TPM_CLEAR_STORED_PASSWORD],
      action.c_str())) {
    chromeos::glib::ScopedError error;
    if (!org_chromium_CryptohomeInterface_tpm_clear_stored_password(
        proxy.gproxy(),
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmClearStoredPassword call failed: %s.\n", error->message);
    }
  } else if (!strcmp(
      switches::kActions[switches::ACTION_INSTALL_ATTRIBUTES_GET],
      action.c_str())) {
    std::string name;
    if (!GetAttrName(cl, &name)) {
      printf("No attribute name specified.\n");
      return 1;
    }

    chromeos::glib::ScopedError error;
    gboolean result;
    if (!org_chromium_CryptohomeInterface_install_attributes_is_ready(
        proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("IsReady call failed: %s.\n", error->message);
    }
    if (result == FALSE) {
      printf("InstallAttributes() is not ready.\n");
      return 1;
    }

    GArray *value = NULL;
    if (!org_chromium_CryptohomeInterface_install_attributes_get(
        proxy.gproxy(),
        name.c_str(),
        &value,
        &result,
        &chromeos::Resetter(&error).lvalue())) {
       printf("Get() failed: %s.\n", error->message);
    }
    std::string value_str(value->data, value->len);
    if (result == TRUE) {
      printf("%s\n", value_str.c_str());
    } else {
      return 1;
    }
    g_array_free(value, false);
  } else if (!strcmp(
      switches::kActions[switches::ACTION_INSTALL_ATTRIBUTES_SET],
      action.c_str())) {
    std::string name;
    if (!GetAttrName(cl, &name)) {
      printf("No attribute name specified.\n");
      return 1;
    }
    std::string value;
    if (!GetAttrValue(cl, &value)) {
      printf("No attribute value specified.\n");
      return 1;
    }

    chromeos::glib::ScopedError error;
    gboolean result;
    if (!org_chromium_CryptohomeInterface_install_attributes_is_ready(
        proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("IsReady call failed: %s.\n", error->message);
    }

    if (result == FALSE) {
      printf("InstallAttributes() is not ready.\n");
      return 1;
    }

    GArray *value_ary = g_array_new(FALSE, FALSE, 1);
    g_array_append_vals(value_ary, value.c_str(), value.size() + 1);
    if (!org_chromium_CryptohomeInterface_install_attributes_set(
        proxy.gproxy(),
        name.c_str(),
        value_ary,
        &result,
        &chromeos::Resetter(&error).lvalue())) {
       printf("Set() failed: %s.\n", error->message);
    }
    g_array_free(value_ary, false);
    if (result == FALSE)
      return 1;
  } else if (!strcmp(
      switches::kActions[switches::ACTION_INSTALL_ATTRIBUTES_FINALIZE],
      action.c_str())) {
    chromeos::glib::ScopedError error;
    gboolean result;
    if (!org_chromium_CryptohomeInterface_install_attributes_is_ready(
        proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("IsReady call failed: %s.\n", error->message);
    }
    if (result == FALSE) {
      printf("InstallAttributes is not ready.\n");
      return 1;
    }
    if (!org_chromium_CryptohomeInterface_install_attributes_finalize(
        proxy.gproxy(),
        &result,
        &chromeos::Resetter(&error).lvalue())) {
      printf("Finalize() failed: %s.\n", error->message);
    }
    printf("InstallAttributesFinalize(): %d\n", result);
  } else if (!strcmp(
      switches::kActions[switches::ACTION_TPM_WAIT_OWNERSHIP],
      action.c_str())) {
    return !WaitForTPMOwnership(&proxy);
  } else if (!strcmp(
      switches::kActions[switches::ACTION_PKCS11_INIT],
      action.c_str())) {
    cryptohome::Pkcs11Init pkcs11_init;
    if (!pkcs11_init.InitializePkcs11())
      return 1;
    if (!pkcs11_init.IsUserTokenBroken()) {
      printf("PKCS#11 token looks OK! Not reinitializing.");
      // Sanitize the token directory in case the token was initialized before
      // sanitization was added.
      if (!pkcs11_init.SanitizeTokenDirectory())
        return 1;
      return 0;
    }
    return !pkcs11_init.InitializeToken();
  } else if (!strcmp(
      switches::kActions[switches::ACTION_FORCE_PKCS11_INIT],
      action.c_str())) {
    // First attempt ownership.
    chromeos::glib::ScopedError error;
    if (!org_chromium_CryptohomeInterface_tpm_can_attempt_ownership(
        proxy.gproxy(),
        &chromeos::Resetter(&error).lvalue())) {
      printf("TpmCanAttemptOwnership call failed: %s.\n", error->message);
      return 1;
    }
    // Now wait for ownership to finish. Note that we don't care if this fails
    // since that might happen if the TPM is already owned.
    WaitForTPMOwnership(&proxy);
    // Attempt "forced" Pkcs#11 initialization.
    cryptohome::Pkcs11Init pkcs11_init;
    if (!pkcs11_init.InitializePkcs11())
      return 1;
    return !pkcs11_init.InitializeToken();
  } else if (!strcmp(
      switches::kActions[switches::ACTION_PKCS11_TOKEN_STATUS],
      action.c_str())) {
    cryptohome::Pkcs11Init init;
    if (init.IsUserTokenBroken()) {
      printf("User token looks broken!\n");
      return 1;
    }
    printf("User token looks OK!\n");
  } else if (!strcmp(switches::kActions[switches::ACTION_PKCS11_TERMINATE],
                     action.c_str())) {
    // If no username is specified, proceed with the empty string.
    string user;
    GetUsername(cl, &user);
    chromeos::glib::ScopedError error;
    if (!org_chromium_CryptohomeInterface_pkcs11_terminate(
            proxy.gproxy(),
            user.c_str(),
            &chromeos::Resetter(&error).lvalue())) {
      printf("PKCS #11 terminate call failed: %s.\n", error->message);
    }
  } else {
    printf("Unknown action or no action given.  Available actions:\n");
    for (int i = 0; switches::kActions[i]; i++)
      printf("  --action=%s\n", switches::kActions[i]);
  }
  return 0;
}
