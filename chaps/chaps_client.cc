// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps client. It sends calls to the Chaps daemon via D-Bus.

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/time/time.h>
#include <brillo/syslog_logging.h>

#include "chaps/chaps_proxy.h"
#include "chaps/chaps_utility.h"
#include "chaps/isolate.h"
#include "chaps/token_manager_client.h"

using base::FilePath;
using brillo::SecureBlob;
using chaps::IsolateCredentialManager;
using std::string;
using std::vector;

namespace {

void PrintHelp() {
  printf("Usage: chaps_client COMMAND [ARGUMENTS]\n");
  printf("Commands:\n");
  printf("  --ping : Checks that the Chaps daemon is available.\n");
  printf(
      "  --load --path=<path> --auth=<auth> [--label=<label>]"
      " : Loads the token at the given path.\n");
  printf("  --unload --path=<path> : Unloads the token at the given path.\n");
  printf(
      "  --change_auth --path=<path> --auth=<old_auth> --new_auth=<new_auth>"
      " : Changes authorization data for the token at the given path.\n");
  printf(
      "  --set_log_level=<level> : Sets the chapsd logging level.\n"
      "    Levels: \n      2 - Errors Only\n      1 - Warnings and Errors\n"
      "      0 - Normal\n     -1 - Verbose (Logs PKCS #11 calls.)\n"
      "     -2 - More Verbose (Logs PKCS #11 calls and arguments.)\n");
  printf("  --list : Lists all loaded token paths.\n");
}

void Ping() {
  auto proxy = chaps::ChapsProxyImpl::Create(false /* shadow_at_exit */);
  if (!proxy)
    exit(-1);
  vector<uint64_t> slot_list;
  uint32_t result = proxy->GetSlotList(
      IsolateCredentialManager::GetDefaultIsolateCredential(), true,
      &slot_list);
  if (result != 0) {
    LOG(ERROR) << "Chaps is available but failed to provide a token list.";
    exit(-1);
  }
  LOG(INFO) << "Chaps is available with " << slot_list.size() << " token(s).";
}

// Loads a token given a path and auth data.
void LoadToken(const string& path, const string& auth, const string& label) {
  chaps::TokenManagerClient client;
  int slot_id = -1;
  client.LoadToken(IsolateCredentialManager::GetDefaultIsolateCredential(),
                   FilePath(path), SecureBlob(auth.begin(), auth.end()), label,
                   &slot_id);
  LOG(INFO) << "LoadToken: " << path << " - slot = " << slot_id;
}

// Unloads a token given a path.
void UnloadToken(const string& path) {
  chaps::TokenManagerClient client;
  client.UnloadToken(IsolateCredentialManager::GetDefaultIsolateCredential(),
                     FilePath(path));
  LOG(INFO) << "Sent Event: Logout: " << path;
}

// Changes authorization data for a token at the given path.
void ChangeAuthData(const string& path,
                    const string& auth_old,
                    const string& auth_new) {
  chaps::TokenManagerClient client;
  client.ChangeTokenAuthData(FilePath(path),
                             SecureBlob(auth_old.begin(), auth_old.end()),
                             SecureBlob(auth_new.begin(), auth_new.end()));
  LOG(INFO) << "Sent Event: Change Authorization Data: " << path;
}

// Sets the logging level.
void SetLogLevel(int level) {
  auto proxy = chaps::ChapsProxyImpl::Create(false /* shadow_at_exit */);
  if (!proxy)
    exit(-1);
  proxy->SetLogLevel(level);
}

void ListTokens() {
  auto proxy = chaps::ChapsProxyImpl::Create(false /* shadow_at_exit */);
  if (!proxy)
    exit(-1);
  vector<uint64_t> slot_list;
  uint32_t result = proxy->GetSlotList(
      IsolateCredentialManager::GetDefaultIsolateCredential(), true,
      &slot_list);
  if (result != 0)
    exit(-1);
  chaps::TokenManagerClient client;
  for (size_t i = 0; i < slot_list.size(); ++i) {
    int slot = slot_list[i];
    FilePath path;
    if (client.GetTokenPath(
            IsolateCredentialManager::GetDefaultIsolateCredential(), slot,
            &path)) {
      LOG(INFO) << "Slot " << slot << ": " << path.value();
    } else {
      LOG(INFO) << "Slot " << slot << ": Empty";
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);

  bool ping = cl->HasSwitch("ping");
  bool load =
      (cl->HasSwitch("load") && cl->HasSwitch("path") && cl->HasSwitch("auth"));
  bool unload = cl->HasSwitch("unload") && cl->HasSwitch("path");
  bool change_auth = (cl->HasSwitch("change_auth") && cl->HasSwitch("path") &&
                      cl->HasSwitch("auth") && cl->HasSwitch("new_auth"));
  bool set_log_level = cl->HasSwitch("set_log_level");
  bool list = cl->HasSwitch("list");
  if (ping + load + unload + change_auth + set_log_level + list != 1) {
    PrintHelp();
    exit(-1);
  }
  if (ping) {
    Ping();
  } else if (load) {
    string label = "Default Token";
    if (cl->HasSwitch("label"))
      label = cl->GetSwitchValueASCII("label");
    LoadToken(cl->GetSwitchValueASCII("path"), cl->GetSwitchValueASCII("auth"),
              label);
  } else if (change_auth) {
    ChangeAuthData(cl->GetSwitchValueASCII("path"),
                   cl->GetSwitchValueASCII("auth"),
                   cl->GetSwitchValueASCII("new_auth"));
  } else if (unload) {
    UnloadToken(cl->GetSwitchValueASCII("path"));
  } else if (set_log_level) {
    int level = 0;
    if (!base::StringToInt(cl->GetSwitchValueASCII("set_log_level"), &level)) {
      LOG(ERROR) << "Invalid argument.";
      exit(-1);
    }
    SetLogLevel(level);
  } else if (list) {
    ListTokens();
  }
  return 0;
}
