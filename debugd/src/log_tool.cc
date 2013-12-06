// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "log_tool.h"

#include <glib.h>

#include <base/base64.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_split.h>
#include <base/string_util.h>

#include "anonymizer_tool.h"
#include "process_with_output.h"

namespace debugd {

const char *kShell = "/bin/sh";
const char *kDebugfsGroup = "debugfs-access";

using std::string;
using std::vector;

typedef vector<string> Strings;

// Returns |value| if |value| is a valid UTF-8 string or a base64-encoded
// string of |value| otherwise.
string EnsureUTF8String(const string& value) {
  if (IsStringUTF8(value))
    return value;

  gchar* base64_value = g_base64_encode(
      reinterpret_cast<const guchar*>(value.c_str()), value.length());
  if (base64_value) {
    string encoded_value = "<base64>: ";
    encoded_value += base64_value;
    g_free(base64_value);
    return encoded_value;
  }
  return "<invalid>";
}

struct Log {
  const char *name;
  const char *command;
  const char *user;
  const char *group;
};

// TODO(ellyjones): sandbox. crosbug.com/35122
string Run(const Log& log) {
  string output;
  ProcessWithOutput p;
  string tailed_cmdline = std::string(log.command) + " | tail -c 256K";
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

static const Log common_logs[] = {
  { "CLIENT_ID", "/bin/cat '/home/chronos/Consent To Send Stats'" },
  { "LOGDATE", "/bin/date" },
  { "Xorg.0.log", "/bin/cat /var/log/Xorg.0.log" },
  { "bios_info", "/bin/cat /var/log/bios_info.txt" },
  { "bios_log",
    "/bin/cat /sys/firmware/log "
    "/proc/device-tree/chosen/ap-console-buffer 2> /dev/null" },
  { "bios_times", "/bin/cat /var/log/bios_times.txt" },
  { "board-specific",
        "/usr/share/userfeedback/scripts/get_board_specific_info" },
  { "boot_times", "/bin/cat /tmp/boot-times-sent" },
  { "chrome_system_log", "/bin/cat /var/log/chrome/chrome" },
  { "console-ramoops", "/bin/cat /dev/pstore/console-ramoops" },
  { "cpu", "/usr/bin/uname -p" },
  { "cpuinfo", "/bin/cat /proc/cpuinfo" },
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
  { "ifconfig", "/sbin/ifconfig -a" },
  { "kernel-crashes", "/bin/cat /var/spool/crash/kernel.*.kcrash" },
  { "lsmod", "lsmod" },
  { "lspci", "/usr/sbin/lspci" },
  { "lsusb", "lsusb" },
  {
      "mali_memory",
      "/bin/cat /sys/class/misc/mali0/device/memory 2> /dev/null"
  },
  { "meminfo", "cat /proc/meminfo" },
  { "memory_spd_info", "/bin/cat /var/log/memory_spd_info.txt" },
  { "mount-encrypted", "/bin/cat /var/log/mount-encrypted.log" },
  { "net-diags.net.log", "/bin/cat /var/log/net-diags.net.log" },
  { "netlog", "/usr/share/userfeedback/scripts/getmsgs --last '2 hours'"
              " /var/log/net.log" },
  { "platform_info", "/bin/cat /var/log/platform_info.txt" },
  { "power_supply_info", "/usr/bin/power_supply_info" },
  { "powerd.LATEST", "/bin/cat /var/log/power_manager/powerd.LATEST" },
  { "powerd.PREVIOUS", "/bin/cat /var/log/power_manager/powerd.PREVIOUS" },
  { "powerd.out", "/bin/cat /var/log/powerd.out" },
  // Changed from 'ps ux' to 'ps aux' since we're running as debugd, not chronos
  { "ps", "/bin/ps aux" },
  { "storage_info", "/bin/cat /var/log/storage_info.txt" },
  { "syslog", "/usr/share/userfeedback/scripts/getmsgs --last '2 hours'"
              " /var/log/messages" },
  { "tlsdate", "/bin/cat /var/log/tlsdate.log" },
  { "touchpad", "/opt/google/touchpad/tpcontrol status" },
  { "touchpad_activity", "/opt/google/touchpad/generate_userfeedback alt" },
  { "touch_fw_version", "grep -E"
              " 'synaptics: Touchpad model|chromeos-touch-[a-z]*-update'"
              " /var/log/messages | tail -n 20" },
  { "ui_log", "/usr/share/userfeedback/scripts/get_log /var/log/ui/ui.LATEST" },
  { "uname", "/bin/uname -a" },
  { "update_engine.log", "cat $(ls -1tr /var/log/update_engine | tail -5 | sed"
         " s.^./var/log/update_engine/.)" },
  { "verified boot", "/bin/cat /var/log/debug_vboot_noisy.log" },
  { "vpd_2.0", "/bin/cat /var/log/vpd_2.0.txt" },
  { "wifi_status", "/usr/bin/network_diagnostics --wifi-internal --no-log" },
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

  // Stuff pulled out of the original list. These need access to the running X
  // session, which we'd rather not give to debugd, or return info specific to
  // the current session (in the setsid(2) sense), which is not useful for
  // debugd
  // { "env", "set" },
  // { "setxkbmap", "/usr/bin/setxkbmap -print -query" },
  // { "xrandr", "/usr/bin/xrandr --verbose" }
  { NULL, NULL }
};

static const Log extra_logs[] = {
  { "mm-status", "/usr/bin/modem status" },
  { "network-devices", "/usr/bin/connectivity show devices" },
  { "network-services", "/usr/bin/connectivity show services" },
  { NULL, NULL }
};

static const Log feedback_logs[] = {
  { "mm-status", "/usr/bin/modem status-feedback" },
  { "network-devices", "/usr/bin/connectivity show-feedback devices" },
  { "network-services", "/usr/bin/connectivity show-feedback services" },
  { NULL, NULL }
};

// List of log files that must directly be collected by Chrome. This is because
// debugd is running under a VFS namespace and does not have access to later
// cryptohome mounts.
static const Log user_logs[] = {
  {"chrome_user_log", "log/chrome"},
  {"login-times", "login-times"},
  {"logout-times", "logout-times"},
  { NULL, NULL}
};

LogTool::LogTool() { }
LogTool::~LogTool() { }

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

string LogTool::GetLog(const string& name, DBus::Error& error) { // NOLINT
  string result;
     GetNamedLogFrom(name, common_logs, &result)
  || GetNamedLogFrom(name, extra_logs, &result)
  || GetNamedLogFrom(name, feedback_logs, &result);
  return result;
}

void GetLogsFrom(const struct Log* logs, LogTool::LogMap* map) {
  for (size_t i = 0; logs[i].name; i++)
    (*map)[logs[i].name] = Run(logs[i]);
}

LogTool::LogMap LogTool::GetAllLogs(DBus::Error& error) { // NOLINT
  LogMap result;
  GetLogsFrom(common_logs, &result);
  GetLogsFrom(extra_logs, &result);
  return result;
}

LogTool::LogMap LogTool::GetFeedbackLogs(DBus::Error& error) { // NOLINT
  LogMap result;
  GetLogsFrom(common_logs, &result);
  GetLogsFrom(feedback_logs, &result);
  AnonymizeLogMap(&result);
  return result;
}

LogTool::LogMap LogTool::GetUserLogFiles(DBus::Error& error) {  // NOLINT
  LogMap result;
  for (size_t i = 0; user_logs[i].name; ++i)
    result[user_logs[i].name] = user_logs[i].command;
  return result;
}

void LogTool::AnonymizeLogMap(LogMap* log_map) {
  AnonymizerTool anonymizer;
  for (LogMap::iterator it = log_map->begin();
       it != log_map->end(); ++it) {
    it->second = anonymizer.Anonymize(it->second);
  }
}

};  // namespace debugd
