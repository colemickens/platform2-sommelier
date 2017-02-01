// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Command line tool to parse data. Data is read from stdin as string or
// protobuf and returned through stdout in string or protobuf format. The tool
// is invoked by the authpolicy daemon in a secure sandbox. It is done this way
// since parsing the output is considered insecure.
//
// Usage:
//   authpolicy_parser <command>
//   For a list of commands see constants.h.
//   Each command reads additional arguments from stdin. See code for details.
//
// Logs to syslog.

#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/syslog_logging.h>

#include "authpolicy/constants.h"
#include "authpolicy/pipe_helper.h"
#include "authpolicy/policy/preg_policy_encoder.h"
#include "authpolicy/samba_interface_internal.h"
#include "bindings/authpolicy_containers.pb.h"
#include "bindings/chrome_device_policy.pb.h"
#include "bindings/cloud_policy.pb.h"

namespace ac = authpolicy::constants;
namespace ah = authpolicy::helper;
namespace ap = authpolicy::protos;
namespace ai = authpolicy::internal;
namespace em = enterprise_management;

namespace {

// 'net ads gpo list'' tokens.
const char kGpoToken_Separator[] = "---------------------";
const char kGpoToken_Name[] = "name";
const char kGpoToken_Filesyspath[] = "filesyspath";
const char kGpoToken_VersionUser[] = "version_user";
const char kGpoToken_VersionMachine[] = "version_machine";
const char kGpoToken_Options[] = "options";

struct GpoEntry {
  GpoEntry() { Clear(); }

  void Clear() {
    name.clear();
    filesyspath.clear();
    version_user = 0;
    version_machine = 0;
    gp_flags = ai::kGpFlagInvalid;
  }

  bool IsValid() const {
    return !name.empty() && !filesyspath.empty() &&
           !(version_user == 0 && version_machine == 0) &&
           gp_flags != ai::kGpFlagInvalid;
  }

  bool IsEmpty() const {
    return name.empty() && filesyspath.empty() && version_user == 0 &&
           version_machine == 0 && gp_flags == ai::kGpFlagInvalid;
  }

  void Log() const {
    LOG(INFO) << "  Name:        " << name;
    LOG(INFO) << "  Filesyspath: " << filesyspath;
    LOG(INFO) << "  Version:     " << version_user << " (user) "
                                   << version_machine << " (machine)";
    LOG(INFO) << "  GPFLags:     " << gp_flags;
  }

  std::string name;
  std::string filesyspath;
  unsigned int version_user;
  unsigned int version_machine;
  int gp_flags;
};

void PushGpo(const GpoEntry& gpo,
             ac::PolicyScope scope,
             std::vector<GpoEntry>* gpo_list) {
  if (gpo.IsEmpty())
    return;

  if (!gpo.IsValid()) {
    LOG(INFO) << "Ignoring invalid GPO";
    gpo.Log();
    return;
  }

  // Filter out GPOs we don't need. If version_user == 0, there's no user
  // policy stored in that GPO. Similarly, if version_machine == 0, there's no
  // device policy.
  const char* filter_reason = nullptr;
  switch (scope) {
    case ac::PolicyScope::USER:
      if (gpo.version_user == 0)
        filter_reason = "user version is 0";
      else if (gpo.gp_flags & ai::kGpFlagUserDisabled)
        filter_reason = "user disabled flag is set";
      break;
    case ac::PolicyScope::MACHINE:
      if (gpo.version_machine == 0)
        filter_reason = "machine version is 0";
      else if (gpo.gp_flags & ai::kGpFlagMachineDisabled)
        filter_reason = "machine disabled flag is set";
      break;
  }
  if (filter_reason) {
    LOG(INFO) << "Filtered out GPO (" << filter_reason << ")";
    gpo.Log();
  } else {
    gpo_list->push_back(gpo);
  }
}

// Prints |str| to stdout for the caller of this tool. Returns an exit code that
// indicates success or failure.
int OutputForCaller(const std::string& str) {
  if (!base::WriteFileDescriptor(STDOUT_FILENO, str.c_str(), str.size())) {
    LOG(ERROR) << "Failed to write output for caller";
    return ac::EXIT_CODE_WRITE_OUTPUT_FAILED;
  }
  return ac::EXIT_CODE_OK;
}

// Parses the output of net ads search to get the user's objectGUID and prints
// it to stdout.
int ParseAccountInfo(const std::string& net_out) {
  std::string object_guid, sam_account_name;
  if (!ai::FindToken(net_out, ':', "objectGUID", &object_guid) ||
      !ai::FindToken(net_out, ':', "sAMAccountName", &sam_account_name)) {
    LOG(ERROR) << "Failed to parse account info";
    return ac::EXIT_CODE_FIND_TOKEN_FAILED;
  }

  // Output data as proto blob.
  ap::AccountInfo account_info_proto;
  account_info_proto.set_object_guid(object_guid);
  account_info_proto.set_sam_account_name(sam_account_name);
  std::string account_info_blob;
  if (!account_info_proto.SerializeToString(&account_info_blob)) {
    LOG(ERROR) << "Failed to convert account info proto to string";
    return ac::EXIT_CODE_WRITE_OUTPUT_FAILED;
  }
  return OutputForCaller(account_info_blob);
}

// Parses the output of net ads info to get the domain controller name and
// prints it to stdout.
int ParseDomainControllerName(const std::string& net_out) {
  // Parse output for dc_name in 'LDAP server name: dc_name.some.domain'.
  std::string dc_name;
  if (!ai::FindToken(net_out, ':', "LDAP server name", &dc_name))
    return ac::EXIT_CODE_FIND_TOKEN_FAILED;

  // We're only interested in the part before the dot.
  size_t dot_pos = dc_name.find('.');
  dc_name = dc_name.substr(0, dot_pos);

  return OutputForCaller(dc_name);
}

// Parses the output of net ads workgroup to get the workgroup and prints it to
// stdout.
int ParseWorkgroup(const std::string& net_out) {
  std::string workgroup;
  if (!ai::FindToken(net_out, ':', "Workgroup", &workgroup))
    return ac::EXIT_CODE_FIND_TOKEN_FAILED;

  return OutputForCaller(workgroup);
}

// Parses the output of net ads gpo list to get the list of GPOs. Prints out a
// serialized GpoList blob to stdout.
int ParseGpoList(const std::string& net_out, ac::PolicyScope scope) {
  // Parse net output.
  GpoEntry gpo;
  std::vector<GpoEntry> gpo_list;
  std::vector<std::string> lines = base::SplitString(
      net_out, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  LOG(INFO) << "Parsing GPO list (" << lines.size() << " lines)";
  bool found_separator = false;
  for (const std::string& line : lines) {
    if (line.find(kGpoToken_Separator) == 0) {
      // Separator between entries. Process last gpo if any.
      PushGpo(gpo, scope, &gpo_list);
      gpo.Clear();
      found_separator = true;
      continue;
    }

    // Collect data
    const size_t colon_pos = line.find(":");
    if (colon_pos == std::string::npos || colon_pos + 1 >= line.size())
      continue;
    const std::string key = line.substr(0, colon_pos);
    std::string value = line.substr(colon_pos + 1);
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);

    bool already_set = false;
    bool version_error = false;
    bool flags_error = false;
    if (key == kGpoToken_Name) {
      already_set = !gpo.name.empty();
      gpo.name = value;
    } else if (key == kGpoToken_Filesyspath) {
      already_set = !gpo.filesyspath.empty();
      gpo.filesyspath = value;
    } else if (key == kGpoToken_VersionUser) {
      already_set = gpo.version_user != 0;
      version_error = !ai::ParseGpoVersion(value, &gpo.version_user);
    } else if (key == kGpoToken_VersionMachine) {
      already_set = gpo.version_machine != 0;
      version_error = !ai::ParseGpoVersion(value, &gpo.version_machine);
    } else if (key == kGpoToken_Options) {
      already_set = gpo.gp_flags != ai::kGpFlagInvalid;
      flags_error = !ai::ParseGpFlags(value, &gpo.gp_flags);
    }

    // Sanity check that we don't miss separators between GPOs.
    if (already_set) {
      LOG(ERROR) << "Failed to parse GPO data (bad format)";
      return ac::EXIT_CODE_PARSE_INPUT_FAILED;
    }

    if (version_error) {
      LOG(ERROR) << "Failed to parse GPO version '" << value << "'";
      return ac::EXIT_CODE_PARSE_INPUT_FAILED;
    }

    if (flags_error) {
      LOG(ERROR) << "Failed to parse GP flags '" << value << "'";
      return ac::EXIT_CODE_PARSE_INPUT_FAILED;
    }
  }

  // Just in case there's no separator in the end.
  PushGpo(gpo, scope, &gpo_list);

  if (!found_separator) {
    // This usually happens when something went wrong, e.g. connection error.
    LOG(ERROR) << "Failed to parse GPO data (no separator, did net fail?)";
    LOG(ERROR) << "Net response: " << net_out;
    return ac::EXIT_CODE_PARSE_INPUT_FAILED;
  }

  LOG(INFO) << "Found " << gpo_list.size() << " GPOs.";
  for (size_t n = 0; n < gpo_list.size(); ++n) {
    LOG(INFO) << n + 1 << ")";
    gpo_list[n].Log();
  }

  // Convert to proto.
  ap::GpoList gpo_list_proto;
  for (const GpoEntry& gpo : gpo_list) {
    // Split the filesyspath, e.g.
    //   \\chrome.lan\SysVol\chrome.lan\Policies\{3507856D-...-CF144DC5CC3A}
    // into
    // - the base path (chrome.lan/SysVol) and
    // - the directory (chrome.lan\Policies\...).
    // Note the change from \ to / in base path.
    std::vector<std::string> file_parts = base::SplitString(
        gpo.filesyspath, "\\/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (file_parts.size() < 4 || !file_parts[0].empty() ||
        !file_parts[1].empty()) {
      LOG(ERROR) << "Failed to split filesyspath '" << gpo.filesyspath
                 << "' into service and directory parts";
      return ac::EXIT_CODE_PARSE_INPUT_FAILED;
    }
    std::string basepath = file_parts[2] + "/" + file_parts[3];
    file_parts =
        std::vector<std::string>(file_parts.begin() + 4, file_parts.end());
    std::string directory = base::JoinString(file_parts, "\\");

    ap::GpoEntry* gpo_proto = gpo_list_proto.add_entries();
    gpo_proto->set_name(gpo.name);
    gpo_proto->set_basepath(basepath);
    gpo_proto->set_directory(directory);
  }

  // Output data as proto blob.
  std::string gpo_list_blob;
  if (!gpo_list_proto.SerializeToString(&gpo_list_blob)) {
    LOG(ERROR) << "Failed to convert GPO list proto to string";
    return ac::EXIT_CODE_WRITE_OUTPUT_FAILED;
  }
  return OutputForCaller(gpo_list_blob);
}

// Parses a set of GPO files and assembles a user or device policy proto. Writes
// the serialized policy blob to stdout. |gpo_file_paths_blob| is expected to be
// a serialized |ap::FilePathList| proto blob.
int ParsePreg(const std::string& gpo_file_paths_blob, ac::PolicyScope scope) {
  // Parse FilePathList proto blob.
  ap::FilePathList gpo_file_paths_proto;
  if (!gpo_file_paths_proto.ParseFromString(gpo_file_paths_blob)) {
    LOG(ERROR) << "Failed to parse file paths blob";
    return ac::EXIT_CODE_READ_INPUT_FAILED;
  }

  // Convert to list of base::FilePaths.
  std::vector<base::FilePath> gpo_file_paths;
  for (int n = 0; n < gpo_file_paths_proto.entries_size(); ++n)
    gpo_file_paths.push_back(base::FilePath(gpo_file_paths_proto.entries(n)));

  std::string policy_blob;
  authpolicy::ErrorType error = authpolicy::ERROR_NONE;
  switch (scope) {
    case ac::PolicyScope::USER: {
      // Parse files into a user policy proto.
      em::CloudPolicySettings policy;
      if (!policy::ParsePRegFilesIntoUserPolicy(gpo_file_paths, &policy,
                                                &error)) {
        LOG(ERROR) << error;
        return ac::EXIT_CODE_PARSE_INPUT_FAILED;
      }

      // Serialize user policy proto to string.
      if (!policy.SerializeToString(&policy_blob))
        return ac::EXIT_CODE_WRITE_OUTPUT_FAILED;
      break;
    }
    case ac::PolicyScope::MACHINE: {
      // Parse files into a device policy proto.
      em::ChromeDeviceSettingsProto policy;
      if (!policy::ParsePRegFilesIntoDevicePolicy(gpo_file_paths, &policy,
                                                  &error)) {
        LOG(ERROR) << error;
        return ac::EXIT_CODE_PARSE_INPUT_FAILED;
      }

      // Serialize policy proto to string.
      if (!policy.SerializeToString(&policy_blob))
        return ac::EXIT_CODE_WRITE_OUTPUT_FAILED;
      break;
    }
    default: { LOG(FATAL) << "invalid scope"; }
  }

  // Print the serialized policy proto to stdout.
  return OutputForCaller(policy_blob);
}

}  // namespace

int main(int argc, const char* const* argv) {
  brillo::OpenLog("authpolicy_parser", true);
  brillo::InitLog(brillo::kLogToSyslog);

  // Required for base::SysInfo.
  base::AtExitManager at_exit_manager;

  // Require one argument, one of the ac::kCmdParse* strings.
  if (argc <= 1) {
    LOG(ERROR) << "No command";
    return ac::EXIT_CODE_BAD_COMMAND;
  }
  const char* cmd = argv[1];

  // All commands take additional arguments via stdin.
  std::string stdin_str;
  if (!ah::ReadPipeToString(STDIN_FILENO, &stdin_str)) {
    LOG(ERROR) << "Failed to read stdin";
    return ac::EXIT_CODE_READ_INPUT_FAILED;
  }

  if (strcmp(cmd, ac::kCmdParseDcName) == 0)
    return ParseDomainControllerName(stdin_str);
  if (strcmp(cmd, ac::kCmdParseWorkgroup) == 0)
    return ParseWorkgroup(stdin_str);
  if (strcmp(cmd, ac::kCmdParseAccountInfo) == 0)
    return ParseAccountInfo(stdin_str);
  if (strcmp(cmd, ac::kCmdParseUserGpoList) == 0)
    return ParseGpoList(stdin_str, ac::PolicyScope::USER);
  if (strcmp(cmd, ac::kCmdParseDeviceGpoList) == 0)
    return ParseGpoList(stdin_str, ac::PolicyScope::MACHINE);
  if (strcmp(cmd, ac::kCmdParseUserPreg) == 0)
    return ParsePreg(stdin_str, ac::PolicyScope::USER);
  if (strcmp(cmd, ac::kCmdParseDevicePreg) == 0)
    return ParsePreg(stdin_str, ac::PolicyScope::MACHINE);

  LOG(ERROR) << "Bad command '" << cmd << "'";
  return ac::EXIT_CODE_BAD_COMMAND;
}
