// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/log_tool.h"

#include <memory>
#include <vector>

#include <base/base64.h>
#include <base/files/file_util.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/values.h>

#include <chromeos/dbus/service_constants.h>
#include <shill/dbus-proxies.h>

#include "debugd/src/constants.h"
#include "debugd/src/process_with_output.h"

namespace debugd {

using std::string;

using Strings = std::vector<string>;

namespace {

const char kRoot[] = "root";
const char kShell[] = "/bin/sh";

// Minimum time in seconds needed to allow shill to test active connections.
const int kConnectionTesterTimeoutSeconds = 5;

struct Log {
  const char *name;
  const char *command;
  const char *user;
  const char *group;
  const char *size_cap;  // passed as arg to 'tail'
};

#define CMD_KERNEL_MODULE_PARAMS(module_name) \
    "cd /sys/module/" #module_name "/parameters 2> /dev/null && grep -sH ^ *"

const Log kCommandLogs[] = {
  { "CLIENT_ID", "/usr/bin/metrics_client -i"},
  { "LOGDATE", "/bin/date" },
  { "atrus_logs", "/bin/cat /var/log/atrus.log 2> /dev/null" },
  { "authpolicy", "/bin/cat /var/log/authpolicy.log" },
  { "bios_info", "/bin/cat /var/log/bios_info.txt" },
  { "bios_log",
    "/bin/cat /sys/firmware/log "
    "/proc/device-tree/chosen/ap-console-buffer 2> /dev/null" },
  { "bios_times", "/bin/cat /var/log/bios_times.txt" },
  { "board-specific",
    "/usr/share/userfeedback/scripts/get_board_specific_info" },
  { "buddyinfo", "/bin/cat /proc/buddyinfo" },
  { "cheets_log", "/usr/bin/collect-cheets-logs 2>&1" },
  { "clobber.log", "/bin/cat /var/log/clobber.log 2> /dev/null" },
  { "clobber-state.log", "/bin/cat /var/log/clobber-state.log 2> /dev/null" },
  { "chrome_system_log", "/bin/cat /var/log/chrome/chrome" },
  // There might be more than one record, so grab them all.
  // Plus, for <linux-3.19, it's named "console-ramoops", but for newer
  // versions, it's named "console-ramoops-#".
  { "console-ramoops", "/bin/cat /sys/fs/pstore/console-ramoops* 2>/dev/null" },
  { "cpu", "/usr/bin/uname -p" },
  { "cpuinfo", "/bin/cat /proc/cpuinfo" },
  { "cr50_version", "/bin/cat /var/cache/cr50-version" },
  { "cros_ec",
    "/bin/cat /var/log/cros_ec.previous /var/log/cros_ec.log 2> /dev/null" },
  { "cros_ec_panicinfo",
    "/bin/cat /sys/kernel/debug/cros_ec/panicinfo 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "cros_ec_pdinfo",
    "/bin/cat /sys/kernel/debug/cros_ec/pdinfo 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "dmesg", "/bin/dmesg" },
  { "ec_info", "/bin/cat /var/log/ec_info.txt" },
  { "eventlog", "/bin/cat /var/log/eventlog.txt" },
  {
    "exynos_gem_objects",
    "/bin/cat /sys/kernel/debug/dri/0/exynos_gem_objects 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "font_info", "/usr/share/userfeedback/scripts/font_info" },
  { "sensor_info", "/usr/share/userfeedback/scripts/sensor_info" },
  { "hardware_class", "/usr/bin/crossystem hwid" },
  { "hostname", "/bin/hostname" },
  { "hw_platform", "/usr/bin/uname -i" },
  {
    "i915_gem_gtt",
    "/bin/cat /sys/kernel/debug/dri/0/i915_gem_gtt 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  {
    "i915_gem_objects",
    "/bin/cat /sys/kernel/debug/dri/0/i915_gem_objects 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  {
    "i915_error_state",
    "/usr/bin/xz -c /sys/kernel/debug/dri/0/i915_error_state 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup,
  },
  { "ifconfig", "/bin/ifconfig -a" },
  { "input_devices", "/bin/cat /proc/bus/input/devices" },
  // Information about the wiphy device, such as current channel.
  { "iw_dev", "/usr/sbin/iw dev" },
  // Hardware capabilities of the wiphy device.
  { "iw_list", "/usr/sbin/iw list" },
#if USE_IWLWIFI_DUMP
  { "iwlmvm_module_params", CMD_KERNEL_MODULE_PARAMS(iwlmvm) },
  { "iwlwifi_module_params", CMD_KERNEL_MODULE_PARAMS(iwlwifi) },
#endif  // USE_IWLWIFI_DUMP
  { "kernel-crashes",
    "/bin/cat /var/spool/crash/kernel.*.kcrash 2> /dev/null" },
  { "logcat", "/usr/sbin/android-sh -c '/system/bin/logcat -d'",
    kRoot,
    kRoot,
  },
  { "lsmod", "lsmod" },
  { "lspci", "/usr/sbin/lspci" },
  { "lsusb", "lsusb && lsusb -t" },
  {
    "mali_memory",
    "/bin/cat /sys/class/misc/mali0/device/memory 2> /dev/null"
  },
  { "meminfo", "cat /proc/meminfo" },
  { "memory_spd_info", "/bin/cat /var/log/memory_spd_info.txt" },
  // The sed command finds the EDID blob (starting the line after "value:") and
  // replaces the serial number with all zeroes.
  //
  // The EDID is printed as a hex dump over several lines, each line containing
  // the contents of 16 bytes. The first 16 bytes are broken down as follows:
  //   uint64_t fixed_pattern;      // Always 00 FF FF FF FF FF FF 00.
  //   uint16_t manufacturer_id;    // Manufacturer ID, encoded as PNP IDs.
  //   uint16_t product_code;       // Manufacturer product code, little-endian.
  //   uint32_t serial_number;      // Serial number, little-endian.
  // Source: https://en.wikipedia.org/wiki/EDID#EDID_1.3_data_format
  //
  // The subsequent substitution command looks for the fixed pattern followed by
  // two 32-bit fields (manufacturer + product, serial number). It replaces the
  // latter field with 8 bytes of zeroes.
  //
  // TODO(crbug.com/731133): Remove the sed command once modetest itself can
  // remove serial numbers.
  {
    "modetest",
    "(modetest; modetest -M evdi; modetest -M udl) | "
        "sed -E '/EDID/ {:a;n;/value:/!ba;n;"
        "s/(00f{12}00)([0-9a-f]{8})([0-9a-f]{8})/\\1\\200000000/}'",
    kRoot,
    kRoot,
  },
  { "mount-encrypted", "/bin/cat /var/log/mount-encrypted.log" },
  { "mountinfo", "/bin/cat /proc/1/mountinfo" },
  { "netlog", "/usr/share/userfeedback/scripts/getmsgs /var/log/net.log" },
  // --processes requires root.
  { "netstat", "/sbin/ss --all --query inet --numeric --processes",
    kRoot,
    kRoot,
  },
  {
    "nvmap_iovmm",
    "/bin/cat /sys/kernel/debug/nvmap/iovmm/allocations 2> /dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup,
  },
  { "platform_info", "/bin/cat /var/log/platform_info.txt" },
  { "power_supply_info", "/usr/bin/power_supply_info" },
  { "power_supply_sysfs", "/usr/bin/print_sysfs_power_supply_data" },
  { "powerd.LATEST", "/bin/cat /var/log/power_manager/powerd.LATEST" },
  { "powerd.PREVIOUS", "/bin/cat /var/log/power_manager/powerd.PREVIOUS" },
  { "powerd.out", "/bin/cat /var/log/powerd.out" },
  { "powerwash_count", "/bin/cat /var/log/powerwash_count 2> /dev/null" },
  // Changed from 'ps ux' to 'ps aux' since we're running as debugd,
  // not chronos.
  { "ps", "/bin/ps aux" },
  // /proc/slabinfo is owned by root and has 0400 permission.
  { "slabinfo", "/bin/cat /proc/slabinfo", kRoot, kRoot, },
  { "storage_info", "/bin/cat /var/log/storage_info.txt" },
  { "syslog", "/usr/share/userfeedback/scripts/getmsgs /var/log/messages" },
  { "system_log_stats", "echo 'BLOCK_SIZE=1024'; "
    "find /var/log/ -type f -exec du --block-size=1024 {} + | sort -n -r",
    kRoot, kRoot},
  { "tlsdate", "/bin/cat /var/log/tlsdate.log" },
  { "top thread", "/usr/bin/top -Hb -n 1 | head -n 40"},
  { "top memory", "/usr/bin/top -o \"+%MEM\" -bn 1 | head -n 57"},
  { "touchpad", "/opt/google/touchpad/tpcontrol status" },
  { "touchpad_activity", "/opt/google/input/cmt_feedback alt" },
  { "touch_fw_version", "grep -E"
              " -e 'synaptics: Touchpad model'"
              " -e 'chromeos-[a-z]*-touch-[a-z]*-update'"
              " /var/log/messages | tail -n 20" },
  {
    "tpm-firmware-updater",
    "/usr/share/userfeedback/scripts/getmsgs /var/log/tpm-firmware-updater.log"
  },
  // TODO(jorgelo,mnissler): Don't run this as root.
  // On TPM 1.2 devices this will likely require adding a new user to the 'tss'
  // group.
  // On TPM 2.0 devices 'get_version_info' uses D-Bus and therefore can run as
  // any user.
  { "tpm_version", "/usr/sbin/tpm-manager get_version_info", kRoot, kRoot },
  { "atmel_ts_refs", "/opt/google/touch/scripts/atmel_tools.sh ts r",
    kRoot, kRoot},
  { "atmel_tp_refs", "/opt/google/touch/scripts/atmel_tools.sh tp r",
    kRoot, kRoot},
  { "atmel_ts_deltas", "/opt/google/touch/scripts/atmel_tools.sh ts d",
    kRoot, kRoot},
  { "atmel_tp_deltas", "/opt/google/touch/scripts/atmel_tools.sh tp d",
    kRoot, kRoot},
  {
    "trim",
    "cat /var/lib/trim/stateful_trim_state /var/lib/trim/stateful_trim_data"
  },
  { "ui_log", "/usr/share/userfeedback/scripts/get_log /var/log/ui/ui.LATEST" },
  { "uname", "/bin/uname -a" },
  { "update_engine.log", "cat $(ls -1tr /var/log/update_engine | tail -5 | sed"
                         " s.^./var/log/update_engine/.)" },
  { "uptime", "/usr/bin/cut -d' ' -f1 /proc/uptime" },
  { "verified boot", "/bin/cat /var/log/debug_vboot_noisy.log" },
  { "vmlog.1.LATEST", "/bin/cat /var/log/vmlog/vmlog.1.LATEST" },
  { "vmlog.1.PREVIOUS", "/bin/cat /var/log/vmlog/vmlog.1.PREVIOUS" },
  { "vmlog.LATEST", "/bin/cat /var/log/vmlog/vmlog.LATEST" },
  { "vmlog.PREVIOUS", "/bin/cat /var/log/vmlog/vmlog.PREVIOUS" },
  { "vmstat", "/bin/cat /proc/vmstat" },
  { "vpd_2.0", "/bin/cat /var/log/vpd_2.0.txt" },
  { "wifi_status", "/usr/bin/network_diag --wifi-internal --no-log" },
  { "zram compressed data size",
    "/bin/cat /sys/block/zram0/compr_data_size 2> /dev/null" },
  { "zram original data size",
    "/bin/cat /sys/block/zram0/orig_data_size 2> /dev/null" },
  { "zram total memory used",
    "/bin/cat /sys/block/zram0/mem_used_total 2> /dev/null" },
  { "zram total reads",
    "/bin/cat /sys/block/zram0/num_reads 2> /dev/null" },
  { "zram total writes",
    "/bin/cat /sys/block/zram0/num_writes 2> /dev/null" },
  { "zram new stats names", "/bin/echo orig_size compr_size used_total limit "
    "used_max zero_pages migrated" },
  { "zram new stats values", "/bin/cat /sys/block/zram0/mm_stat 2> /dev/null" },
  { "cros_tp version", "/bin/cat /sys/class/chromeos/cros_tp/version" },
  { "cros_tp console", "/usr/sbin/ectool --name=cros_tp console",
    kRoot, kRoot },
  { "cros_tp frame", "/usr/sbin/ectool --name=cros_tp tpframeget",
    kRoot, kRoot },

  // Stuff pulled out of the original list. These need access to the running X
  // session, which we'd rather not give to debugd, or return info specific to
  // the current session (in the setsid(2) sense), which is not useful for
  // debugd
  // { "env", "set" },
  // { "setxkbmap", "/usr/bin/setxkbmap -print -query" },
  // { "xrandr", "/usr/bin/xrandr --verbose" }
  { nullptr, nullptr }
};

// netstat and logcat should appear in chrome://system but not in feedback
// reports.  Open sockets may have privacy implications, and logcat is
// already incorporated via arc-bugreport.
const std::vector<string> kCommandLogsExclude = {"netstat", "logcat"};

const Log kExtraLogs[] = {
#if USE_CELLULAR
  { "mm-status", "/usr/bin/modem status" },
#endif  // USE_CELLULAR
  { "network-devices", "/usr/bin/connectivity show devices" },
  { "network-services", "/usr/bin/connectivity show services" },
  { nullptr, nullptr }
};

const Log kFeedbackLogs[] = {
#if USE_CELLULAR
  { "mm-status", "/usr/bin/modem status-feedback" },
#endif  // USE_CELLULAR
  { "network-devices", "/usr/bin/connectivity show-feedback devices" },
  { "network-services", "/usr/bin/connectivity show-feedback services" },
  { nullptr, nullptr }
};

// List of log files needed to be part of the feedback report that are huge and
// must be sent back to the client via the file descriptor using
// LogTool::GetBigFeedbackLogs().
const Log kBigFeedbackLogs[] = {
  { "arc-bugreport",
    "/bin/cat /run/arc/bugreport/pipe 2> /dev/null",
    // ARC bugreport permissions are weird. Since we're just running cat, this
    // shouldn't cause any issues.
    kRoot,
    kRoot,
    "10M",
  },
  { nullptr, nullptr }
};

// List of log files that must directly be collected by Chrome. This is because
// debugd is running under a VFS namespace and does not have access to later
// cryptohome mounts.
const Log kUserLogs[] = {
  {"chrome_user_log", "log/chrome"},
  {"login-times", "login-times"},
  {"logout-times", "logout-times"},
  { nullptr, nullptr}
};

// Returns |value| if |value| is a valid UTF-8 string or a base64-encoded
// string of |value| otherwise.
string EnsureUTF8String(const string& value) {
  if (base::IsStringUTF8(value))
    return value;

  string encoded_value;
  base::Base64Encode(value, &encoded_value);
  return "<base64>: " + encoded_value;
}

// TODO(ellyjones): sandbox. crosbug.com/35122
string Run(const Log& log) {
  string output;
  ProcessWithOutput p;
  string tailed_cmdline = string(log.command) + " | tail -c " +
                          (log.size_cap ? log.size_cap : "512K");
  if (log.user && log.group)
    p.SandboxAs(log.user, log.group);
  if (!p.Init())
    return "<not available>";
  p.AddArg(kShell);
  p.AddStringOption("-c", tailed_cmdline);
  if (p.Run())
    return "<not available>";
  p.GetOutput(&output);
  if (!output.size())
    return "<empty>";
  return EnsureUTF8String(output);
}

// Fills |dictionary| with the anonymized contents of the logs in |logs|.
void GetLogsInDictionary(const struct Log* logs,
                         AnonymizerTool* anonymizer,
                         base::DictionaryValue* dictionary) {
  for (size_t i = 0; logs[i].name; ++i) {
    dictionary->SetStringWithoutPathExpansion(
        logs[i].name, anonymizer->Anonymize(Run(logs[i])));
  }
}

// Serializes the |dictionary| into the file with the given |fd| in a JSON
// format.
void SerializeLogsAsJSON(const base::DictionaryValue& dictionary,
                         const dbus::FileDescriptor& fd) {
  string logs_json;
  base::JSONWriter::WriteWithOptions(dictionary,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &logs_json);
  base::WriteFileDescriptor(fd.value(), logs_json.c_str(), logs_json.size());
}

bool GetNamedLogFrom(const string& name, const struct Log* logs,
                     string* result) {
  for (size_t i = 0; logs[i].name; i++) {
    if (name == logs[i].name) {
      *result = Run(logs[i]);
      return true;
    }
  }
  *result = "<invalid log name>";
  return false;
}

void GetLogsFrom(const struct Log* logs, LogTool::LogMap* map) {
  for (size_t i = 0; logs[i].name; i++)
    (*map)[logs[i].name] = Run(logs[i]);
}

}  // namespace

void LogTool::CreateConnectivityReport() {
  // Perform ConnectivityTrial to report connection state in feedback log.
  auto shill = std::make_unique<org::chromium::flimflam::ManagerProxy>(bus_);
  // Give the connection trial time to test the connection and log the results
  // before collecting the logs for feedback.
  // TODO(silberst): Replace the simple approach of a single timeout with a more
  // coordinated effort.
  if (shill && shill->CreateConnectivityReport(nullptr))
    sleep(kConnectionTesterTimeoutSeconds);
}

string LogTool::GetLog(const string& name) {
  string result;
     GetNamedLogFrom(name, kCommandLogs, &result)
  || GetNamedLogFrom(name, kExtraLogs, &result)
  || GetNamedLogFrom(name, kFeedbackLogs, &result);
  return result;
}

LogTool::LogMap LogTool::GetAllLogs() {
  CreateConnectivityReport();
  LogMap result;
  GetLogsFrom(kCommandLogs, &result);
  GetLogsFrom(kExtraLogs, &result);
  return result;
}

LogTool::LogMap LogTool::GetFeedbackLogs() {
  CreateConnectivityReport();
  LogMap result;
  GetLogsFrom(kCommandLogs, &result);
  for (const auto& key : kCommandLogsExclude) {
    result.erase(key);
  }
  GetLogsFrom(kFeedbackLogs, &result);
  AnonymizeLogMap(&result);
  return result;
}

void LogTool::GetBigFeedbackLogs(const dbus::FileDescriptor& fd) {
  CreateConnectivityReport();
  base::DictionaryValue dictionary;
  GetLogsInDictionary(kCommandLogs, &anonymizer_, &dictionary);
  for (const auto& key : kCommandLogsExclude) {
    dictionary.Remove(key, nullptr);
  }
  GetLogsInDictionary(kFeedbackLogs, &anonymizer_, &dictionary);
  GetLogsInDictionary(kBigFeedbackLogs, &anonymizer_, &dictionary);
  SerializeLogsAsJSON(dictionary, fd);
}

LogTool::LogMap LogTool::GetUserLogFiles() {
  LogMap result;
  for (size_t i = 0; kUserLogs[i].name; ++i)
    result[kUserLogs[i].name] = kUserLogs[i].command;
  return result;
}

void LogTool::AnonymizeLogMap(LogMap* log_map) {
  for (auto& entry : *log_map)
    entry.second = anonymizer_.Anonymize(entry.second);
}

}  // namespace debugd
