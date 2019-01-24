// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/log_tool.h"

#include <lzma.h>
#include <memory>
#include <string>
#include <vector>

#include <base/base64.h>
#include <base/files/file_util.h>
#include <base/json/json_writer.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/utf_string_conversion_utils.h>
#include <base/values.h>

#include <chromeos/dbus/service_constants.h>
#include <shill/dbus-proxies.h>

#include "debugd/src/constants.h"
#include "debugd/src/perf_tool.h"
#include "debugd/src/process_with_output.h"

#include "brillo/key_value_store.h"
#include <brillo/osrelease_reader.h>

namespace debugd {

using std::string;

using Strings = std::vector<string>;

namespace {

const char kRoot[] = "root";
const char kShell[] = "/bin/sh";
constexpr char kLsbReleasePath[] = "/etc/lsb-release";

// Minimum time in seconds needed to allow shill to test active connections.
const int kConnectionTesterTimeoutSeconds = 5;

// Default maximum size of a log entry.
constexpr const char kDefaultSizeCap[] = "512K";

// Default running perf for 2 seconds.
constexpr const int kPerfDurationSecs = 2;

struct Log {
  const char* name;
  const char* command;
  const char* user = nullptr;
  const char* group = nullptr;
  const char* size_cap = kDefaultSizeCap;  // passed as arg to 'tail -c'
  LogTool::Encoding encoding = LogTool::Encoding::kAutodetect;
};

#define CMD_KERNEL_MODULE_PARAMS(module_name) \
    "cd /sys/module/" #module_name "/parameters 2>/dev/null && grep -sH ^ *"

const Log kCommandLogs[] = {
  // We need to enter init's mount namespace because it has /home/chronos
  // mounted which is where the consent knob lives.  We don't have that mount
  // in our own mount namespace (by design).  https://crbug.com/884249
  { "CLIENT_ID", "/usr/bin/nsenter -t1 -m /usr/bin/metrics_client -i",
    kRoot,
    kDebugfsGroup,
  },
  { "LOGDATE", "/bin/date" },
  { "atrus_logs", "cat /var/log/atrus.log 2>/dev/null" },
  { "authpolicy", "cat /var/log/authpolicy.log" },
  { "bootstat_summary", "/usr/bin/bootstat_summary"},
  { "bio_crypto_init.log", "cat /var/log/bio_crypto_init/bio_crypto_init.log" },
  { "biod.LATEST", "cat /var/log/biod/biod.LATEST" },
  { "biod.PREVIOUS", "cat /var/log/biod/biod.PREVIOUS" },
  { "bios_info", "cat /var/log/bios_info.txt" },
  { "bios_log",
    "cat /sys/firmware/log "
    "/proc/device-tree/chosen/ap-console-buffer 2>/dev/null" },
  { "bios_times", "cat /var/log/bios_times.txt" },
  { "board-specific",
    "/usr/share/userfeedback/scripts/get_board_specific_info" },
  { "buddyinfo", "cat /proc/buddyinfo" },
  { "cbi_info", "/usr/share/userfeedback/scripts/cbi_info", kRoot, kRoot},
  { "cheets_log", "cat /var/log/arc.log 2>/dev/null" },
  { "clobber.log", "cat /var/log/clobber.log 2>/dev/null" },
  { "clobber-state.log", "cat /var/log/clobber-state.log 2>/dev/null" },
  { "chrome_system_log", "cat /var/log/chrome/chrome" },
  { "chrome_system_log.PREVIOUS", "cat /var/log/chrome/chrome.PREVIOUS" },
  // There might be more than one record, so grab them all.
  // Plus, for <linux-3.19, it's named "console-ramoops", but for newer
  // versions, it's named "console-ramoops-#".
  { "console-ramoops", "cat /sys/fs/pstore/console-ramoops* 2>/dev/null" },
  { "cpuinfo", "cat /proc/cpuinfo" },
  { "cr50_version", "cat /var/cache/cr50-version" },
  { "cros_ec",
    "cat /var/log/cros_ec.previous /var/log/cros_ec.log 2>/dev/null" },
  { "cros_ec_panicinfo",
    "cat /sys/kernel/debug/cros_ec/panicinfo 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "cros_ec_pdinfo",
    "cat /sys/kernel/debug/cros_ec/pdinfo 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "cros_fp",
    "cat /var/log/cros_fp.previous /var/log/cros_fp.log 2>/dev/null" },
  { "dmesg", "/bin/dmesg" },
  { "ec_info", "cat /var/log/ec_info.txt" },
  // The sed command replaces the EDID serial number (4 bytes at position 12)
  // with zeroes. See https://en.wikipedia.org/wiki/EDID#EDID_1.4_data_format.
  { "edid-decode",
    "for f in /sys/class/drm/card0-*/edid; do "
        "echo \"----------- ${f}\"; "
        "sed -E 's/^(.{11}).{4}/\\1\\x0\\x0\\x0\\x0/' \"${f}\" | "
        // edid-decode's stderr output is redundant, so silence it.
        "edid-decode 2>/dev/null; "
        "done"
  },
  { "eventlog", "cat /var/log/eventlog.txt" },
  {
    "exynos_gem_objects",
    "cat /sys/kernel/debug/dri/0/exynos_gem_objects 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "font_info", "/usr/share/userfeedback/scripts/font_info" },
  { "sensor_info", "/usr/share/userfeedback/scripts/sensor_info" },
  { "hammerd", "cat /var/log/hammerd.log 2>/dev/null" },
  { "hardware_class", "/usr/bin/crossystem hwid" },
  { "hostname", "/bin/hostname" },
  {
    "i915_gem_gtt",
    "cat /sys/kernel/debug/dri/0/i915_gem_gtt 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  {
    "i915_gem_objects",
    "cat /sys/kernel/debug/dri/0/i915_gem_objects 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  {
    "i915_error_state",
    "/usr/bin/xz -c /sys/kernel/debug/dri/0/i915_error_state 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup,
    kDefaultSizeCap,
    LogTool::Encoding::kBinary,
  },
  { "ifconfig", "/bin/ifconfig -a" },
  { "input_devices", "cat /proc/bus/input/devices" },
  // Information about the wiphy device, such as current channel.
  { "iw_dev", "/usr/sbin/iw dev" },
  // Hardware capabilities of the wiphy device.
  { "iw_list", "/usr/sbin/iw list" },
#if USE_IWLWIFI_DUMP
  { "iwlmvm_module_params", CMD_KERNEL_MODULE_PARAMS(iwlmvm) },
  { "iwlwifi_module_params", CMD_KERNEL_MODULE_PARAMS(iwlwifi) },
#endif  // USE_IWLWIFI_DUMP
  { "kernel-crashes", "cat /var/spool/crash/kernel.*.kcrash 2>/dev/null" },
  { "logcat",
    "/usr/sbin/android-sh -c '/system/bin/logcat -d'",
    kRoot,
    kRoot,
    kDefaultSizeCap,
    LogTool::Encoding::kUtf8,
  },
  { "lsmod", "lsmod" },
  { "lspci", "/usr/sbin/lspci" },
  { "lsusb", "lsusb && lsusb -t" },
  { "mali_memory", "cat /sys/class/misc/mali0/device/memory 2>/dev/null" },
  { "memd.parameters", "cat /var/log/memd/memd.parameters 2>/dev/null" },
  { "memd clips", "cat /var/log/memd/memd.clip* 2>/dev/null" },
  { "meminfo", "cat /proc/meminfo" },
  {
    "memory_spd_info",
    // mosys may use 'i2c-dev', which may not be loaded yet.
    "modprobe i2c-dev 2>/dev/null && "
        "mosys -l memory spd print all 2>/dev/null",
    kRoot,
    kDebugfsGroup,
  },
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
  { "mount-encrypted", "cat /var/log/mount-encrypted.log" },
  { "mountinfo", "cat /proc/1/mountinfo" },
  { "netlog", "/usr/share/userfeedback/scripts/getmsgs /var/log/net.log" },
  // --processes requires root.
  { "netstat", "/sbin/ss --all --query inet --numeric --processes",
    kRoot,
    kRoot,
  },
  {
    "nvmap_iovmm",
    "cat /sys/kernel/debug/nvmap/iovmm/allocations 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup,
  },
  { "oemdata", "/usr/share/cros/oemdata.sh", kRoot, kRoot, },
  { "pagetypeinfo", "cat /proc/pagetypeinfo" },
  {
    "platform_info",
    // mosys may use 'i2c-dev', which may not be loaded yet.
    "modprobe i2c-dev 2>/dev/null && "
        "for param in "
          "vendor "
          "name "
          "version "
          "family "
          "model "
          "sku "
          "customization "
        "; do "
          "mosys -l platform \"${param}\" 2>/dev/null; "
        "done",
    kRoot,
    kDebugfsGroup,
  },
  { "power_supply_info", "/usr/bin/power_supply_info" },
  { "power_supply_sysfs", "/usr/bin/print_sysfs_power_supply_data" },
  { "powerd.LATEST", "cat /var/log/power_manager/powerd.LATEST" },
  { "powerd.PREVIOUS", "cat /var/log/power_manager/powerd.PREVIOUS" },
  { "powerd.out", "cat /var/log/powerd.out" },
  { "powerwash_count", "cat /var/log/powerwash_count 2>/dev/null" },
  // Changed from 'ps ux' to 'ps aux' since we're running as debugd,
  // not chronos.
  { "ps", "/bin/ps aux" },
  // /proc/slabinfo is owned by root and has 0400 permission.
  { "slabinfo", "cat /proc/slabinfo", kRoot, kRoot, },
  { "storage_info", "cat /var/log/storage_info.txt" },
  { "swap_info",
    "/usr/share/cros/init/swap.sh status 2>/dev/null",
    SandboxedProcess::kDefaultUser,
    kDebugfsGroup
  },
  { "syslog", "/usr/share/userfeedback/scripts/getmsgs /var/log/messages" },
  { "system_log_stats", "echo 'BLOCK_SIZE=1024'; "
    "find /var/log/ -type f -exec du --block-size=1024 {} + | sort -n -r",
    kRoot, kRoot},
  { "threads", "/bin/ps -T axo pid,ppid,spid,pcpu,ni,stat,time,comm" },
  { "tlsdate", "cat /var/log/tlsdate.log" },
  { "top thread", "/usr/bin/top -Hb -n 1 | head -n 40"},
  { "top memory", "/usr/bin/top -o \"+%MEM\" -bn 1 | head -n 57"},
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
  { "verified boot", "cat /var/log/debug_vboot_noisy.log" },
  { "vmlog.1.LATEST", "cat /var/log/vmlog/vmlog.1.LATEST" },
  { "vmlog.1.PREVIOUS", "cat /var/log/vmlog/vmlog.1.PREVIOUS" },
  { "vmlog.LATEST", "cat /var/log/vmlog/vmlog.LATEST" },
  { "vmlog.PREVIOUS", "cat /var/log/vmlog/vmlog.PREVIOUS" },
  { "vmstat", "cat /proc/vmstat" },
  { "vpd_2.0", "cat /var/log/vpd_2.0.txt" },
  { "wifi_status", "/usr/bin/network_diag --wifi-internal --no-log" },
  { "zram compressed data size",
    "cat /sys/block/zram0/compr_data_size 2>/dev/null" },
  { "zram original data size",
    "cat /sys/block/zram0/orig_data_size 2>/dev/null" },
  { "zram total memory used",
    "cat /sys/block/zram0/mem_used_total 2>/dev/null" },
  { "zram total reads",
    "cat /sys/block/zram0/num_reads 2>/dev/null" },
  { "zram total writes",
    "cat /sys/block/zram0/num_writes 2>/dev/null" },
  { "zram new stats names",
    "echo orig_size compr_size used_total limit "
    "used_max zero_pages migrated" },
  { "zram new stats values", "cat /sys/block/zram0/mm_stat 2>/dev/null" },
  { "cros_tp version", "cat /sys/class/chromeos/cros_tp/version" },
  { "cros_tp console", "/usr/sbin/ectool --name=cros_tp console",
    kRoot, kRoot },
  { "cros_tp frame", "/usr/sbin/ectool --name=cros_tp tpframeget",
    kRoot, kRoot },
  { "crostini", "/usr/bin/cicerone_client --get_info" },

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
    "cat /run/arc/bugreport/pipe 2>/dev/null",
    // ARC bugreport permissions are weird. Since we're just running cat, this
    // shouldn't cause any issues.
    kRoot,
    kRoot,
    "10M",
    LogTool::Encoding::kUtf8,
  },
  { nullptr, nullptr }
};

// List of log files that must directly be collected by Chrome. This is because
// debugd is running under a VFS namespace and does not have access to later
// cryptohome mounts.
const Log kUserLogs[] = {
  {"chrome_user_log", "log/chrome"},
  {"libassistant_user_log", "log/libassistant.log"},
  {"login-times", "login-times"},
  {"logout-times", "logout-times"},
  { nullptr, nullptr}
};

// TODO(ellyjones): sandbox. crosbug.com/35122
string Run(const Log& log) {
  string output;
  ProcessWithOutput p;
  string tailed_cmdline =
      base::StringPrintf("%s | tail -c %s", log.command, log.size_cap);
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
  return LogTool::EnsureUTF8String(output, log.encoding);
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
                         const base::ScopedFD& fd) {
  string logs_json;
  base::JSONWriter::WriteWithOptions(dictionary,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &logs_json);
  base::WriteFileDescriptor(fd.get(), logs_json.c_str(), logs_json.size());
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

void GetLsbReleaseInfo(LogTool::LogMap* map) {
  const base::FilePath lsb_release(kLsbReleasePath);
  brillo::KeyValueStore store;
  if (!store.Load(lsb_release)) {
    // /etc/lsb-release might not be present (cros deploying a new
    // configuration or no fields set at all). Just print a debug
    // message and continue.
    DLOG(INFO) << "Could not load fields from " << lsb_release.value();
  } else {
    for (const auto& key : store.GetKeys()) {
      std::string value;
      store.GetString(key, &value);
      (*map)[key] = value;
    }
  }
}

void GetOsReleaseInfo(LogTool::LogMap* map) {
  brillo::OsReleaseReader reader;
  reader.Load();
  for (const auto& key : reader.GetKeys()) {
    std::string value;
    reader.GetString(key, &value);
    (*map)["os-release " + key] = value;
  }
}

void PopulateDictionaryValue(const LogTool::LogMap& map,
                             base::DictionaryValue* dictionary) {
  for (const auto& kv : map) {
    dictionary->SetString(kv.first, kv.second);
  }
}

bool CompressXzBuffer(const std::vector<uint8_t>& in_buffer,
                      std::vector<uint8_t>* out_buffer) {
  size_t out_size = lzma_stream_buffer_bound(in_buffer.size());
  out_buffer->resize(out_size);
  size_t out_pos = 0;

  lzma_ret ret = lzma_easy_buffer_encode(
      LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64, nullptr, in_buffer.data(),
      in_buffer.size(), out_buffer->data(), &out_pos, out_size);

  if (ret != LZMA_OK) {
    out_buffer->clear();
    return false;
  }

  out_buffer->resize(out_pos);
  return true;
}

void GetPerfData(LogTool::LogMap* map) {
  // Run perf to collect system-wide performance profile when user triggers
  // feedback report. Perf runs at sampling frequency of ~500 hz (499 is used
  // to avoid sampling periodic system activities), with callstack in each
  // sample (-g).
  std::vector<std::string> perf_args = {
    "perf", "record", "-a", "-g", "-F", "499"
  };
  std::vector<uint8_t> perf_data;
  int32_t status;

  debugd::PerfTool perf_tool;
  if (!perf_tool.GetPerfOutput(kPerfDurationSecs, perf_args, &perf_data,
                               nullptr, &status, nullptr))
    return;

  // XZ compress the profile data.
  std::vector<uint8_t> perf_data_xz;
  if (!CompressXzBuffer(perf_data, &perf_data_xz))
    return;

  // Base64 encode the compressed data.
  std::string perf_data_str(reinterpret_cast<const char*>(perf_data_xz.data()),
                            perf_data_xz.size());
  (*map)["perf-data"] =
      LogTool::EnsureUTF8String(perf_data_str, LogTool::Encoding::kBinary);
}

}  // namespace

void LogTool::CreateConnectivityReport(bool wait_for_results) {
  // Perform ConnectivityTrial to report connection state in feedback log.
  auto shill = std::make_unique<org::chromium::flimflam::ManagerProxy>(bus_);
  // Give the connection trial time to test the connection and log the results
  // before collecting the logs for feedback.
  // TODO(silberst): Replace the simple approach of a single timeout with a more
  // coordinated effort.
  if (shill && shill->CreateConnectivityReport(nullptr) && wait_for_results)
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
  CreateConnectivityReport(false);
  LogMap result;
  GetLogsFrom(kCommandLogs, &result);
  GetLogsFrom(kExtraLogs, &result);
  GetLsbReleaseInfo(&result);
  GetOsReleaseInfo(&result);
  return result;
}

LogTool::LogMap LogTool::GetAllDebugLogs() {
  CreateConnectivityReport(true);
  LogMap result;
  GetLogsFrom(kCommandLogs, &result);
  GetLogsFrom(kExtraLogs, &result);
  GetLogsFrom(kBigFeedbackLogs, &result);
  GetLsbReleaseInfo(&result);
  GetOsReleaseInfo(&result);
  return result;
}

LogTool::LogMap LogTool::GetFeedbackLogs() {
  CreateConnectivityReport(true);
  LogMap result;
  GetLogsFrom(kCommandLogs, &result);
  for (const auto& key : kCommandLogsExclude) {
    result.erase(key);
  }
  GetLogsFrom(kFeedbackLogs, &result);
  GetLsbReleaseInfo(&result);
  GetOsReleaseInfo(&result);
  AnonymizeLogMap(&result);
  return result;
}

void LogTool::GetBigFeedbackLogs(const base::ScopedFD& fd) {
  CreateConnectivityReport(true);
  LogMap map;
  GetPerfData(&map);
  base::DictionaryValue dictionary;
  GetLogsInDictionary(kCommandLogs, &anonymizer_, &dictionary);
  for (const auto& key : kCommandLogsExclude) {
    dictionary.Remove(key, nullptr);
  }
  GetLogsInDictionary(kFeedbackLogs, &anonymizer_, &dictionary);
  GetLogsInDictionary(kBigFeedbackLogs, &anonymizer_, &dictionary);
  GetLsbReleaseInfo(&map);
  GetOsReleaseInfo(&map);
  PopulateDictionaryValue(map, &dictionary);
  SerializeLogsAsJSON(dictionary, fd);
}

LogTool::LogMap LogTool::GetUserLogFiles() {
  LogMap result;
  for (size_t i = 0; kUserLogs[i].name; ++i)
    result[kUserLogs[i].name] = kUserLogs[i].command;
  return result;
}

// static
string LogTool::EnsureUTF8String(const string& value,
                                 LogTool::Encoding source_encoding) {
  if (source_encoding == LogTool::Encoding::kAutodetect) {
    if (base::IsStringUTF8(value))
      return value;
    source_encoding = LogTool::Encoding::kBinary;
  }

  if (source_encoding == LogTool::Encoding::kUtf8) {
    string output;
    const char* src = value.data();
    int32_t src_len = static_cast<int32_t>(value.length());

    output.reserve(value.size());
    for (int32_t char_index = 0; char_index < src_len; char_index++) {
      uint32_t code_point;
      if (!base::ReadUnicodeCharacter(src, src_len, &char_index, &code_point) ||
          !base::IsValidCharacter(code_point)) {
        // Replace invalid characters with U+FFFD REPLACEMENT CHARACTER.
        code_point = 0xFFFD;
      }
      base::WriteUnicodeCharacter(code_point, &output);
    }
    return output;
  } else {
    string encoded_value;
    base::Base64Encode(value, &encoded_value);
    return "<base64>: " + encoded_value;
  }
}

void LogTool::AnonymizeLogMap(LogMap* log_map) {
  for (auto& entry : *log_map)
    entry.second = anonymizer_.Anonymize(entry.second);
}

}  // namespace debugd
