// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/log_tool.h"

#include <grp.h>
#include <inttypes.h>
#include <lzma.h>
#include <pwd.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/base64.h>
#include <base/files/file.h>
#include <base/files/file_path.h>
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

// Default running perf for 2 seconds.
constexpr const int kPerfDurationSecs = 2;
// TODO(chinglinyu) Remove after crbug/934702 is fixed.
// The following description is added to 'perf-data' as a temporary solution
// before the update of feedback disclosure to users is done in crbug/934702.
constexpr const char kPerfDataDescription[] =
    "perf-data contains performance profiling information about how much time "
    "the system spends on various activities (program execution stack traces). "
    "This might reveal some information about what system features and "
    "resources are being used. The full detail of perf-data can be found in "
    "the PerfDataProto protocol buffer message type in the chromium source "
    "repository.\n";

#define CMD_KERNEL_MODULE_PARAMS(module_name) \
    "cd /sys/module/" #module_name "/parameters 2>/dev/null && grep -sH ^ *"

using Log = LogTool::Log;
constexpr Log::LogType kCommand = Log::kCommand;
constexpr Log::LogType kFile = Log::kFile;
const std::vector<Log> kCommandLogs {
  // We need to enter init's mount namespace because it has /home/chronos
  // mounted which is where the consent knob lives.  We don't have that mount
  // in our own mount namespace (by design).  https://crbug.com/884249
  Log(kCommand, "CLIENT_ID",
      "/usr/bin/nsenter -t1 -m /usr/bin/metrics_client -i", kRoot,
      kDebugfsGroup),
  Log(kCommand, "LOGDATE", "/bin/date"),
  Log(kFile, "atrus_logs", "/var/log/atrus.log"),
  Log(kFile, "authpolicy", "/var/log/authpolicy.log"),
  Log(kCommand, "bootstat_summary", "/usr/bin/bootstat_summary"),
  Log(kFile, "bio_crypto_init.log",
      "/var/log/bio_crypto_init/bio_crypto_init.log"),
  Log(kFile, "biod.LATEST", "/var/log/biod/biod.LATEST"),
  Log(kFile, "biod.PREVIOUS", "/var/log/biod/biod.PREVIOUS"),
  Log(kFile, "bios_info", "/var/log/bios_info.txt"),
  Log(kCommand, "bios_log",
      "cat /sys/firmware/log "
      "/proc/device-tree/chosen/ap-console-buffer 2>/dev/null"),
  Log(kFile, "bios_times", "/var/log/bios_times.txt"),
  Log(kCommand, "board-specific",
      "/usr/share/userfeedback/scripts/get_board_specific_info"),
  Log(kFile, "buddyinfo", "/proc/buddyinfo"),
  Log(kCommand, "cbi_info", "/usr/share/userfeedback/scripts/cbi_info", kRoot,
      kRoot),
  Log(kFile, "cheets_log", "/var/log/arc.log"),
  Log(kFile, "clobber.log", "/var/log/clobber.log"),
  Log(kFile, "clobber-state.log", "/var/log/clobber-state.log"),
  Log(kFile, "chrome_system_log", "/var/log/chrome/chrome"),
  Log(kFile, "chrome_system_log.PREVIOUS", "/var/log/chrome/chrome.PREVIOUS"),
  // There might be more than one record, so grab them all.
  // Plus, for <linux-3.19, it's named "console-ramoops", but for newer
  // versions, it's named "console-ramoops-#".
  Log(kCommand, "console-ramoops",
      "cat /sys/fs/pstore/console-ramoops* 2>/dev/null"),
  Log(kFile, "cpuinfo", "/proc/cpuinfo"),
  Log(kFile, "cr50_version", "/var/cache/cr50-version"),
  Log(kFile, "cros_ec.log", "/var/log/cros_ec.log"),
  Log(kFile, "cros_ec.previous", "/var/log/cros_ec.previous"),
  Log(kFile, "cros_ec_panicinfo", "/sys/kernel/debug/cros_ec/panicinfo",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kFile, "cros_ec_pdinfo", "/sys/kernel/debug/cros_ec/pdinfo",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kFile, "cros_fp.previous", "/var/log/cros_fp.previous"),
  Log(kFile, "cros_fp.log", "/var/log/cros_fp.log"),
  Log(kCommand, "dmesg", "/bin/dmesg"),
  Log(kFile, "ec_info", "/var/log/ec_info.txt"),
  // The sed command replaces the EDID serial number (4 bytes at position 12)
  // with zeroes. See https://en.wikipedia.org/wiki/EDID#EDID_1.4_data_format.
  Log(kCommand, "edid-decode",
      "for f in /sys/class/drm/card0-*/edid; do "
        "echo \"----------- ${f}\"; "
        "sed -E 's/^(.{11}).{4}/\\1\\x0\\x0\\x0\\x0/' \"${f}\" | "
        // edid-decode's stderr output is redundant, so silence it.
        "edid-decode 2>/dev/null; "
      "done"),
  Log(kFile, "eventlog", "/var/log/eventlog.txt"),
  Log(kFile, "exynos_gem_objects", "/sys/kernel/debug/dri/0/exynos_gem_objects",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kCommand, "font_info", "/usr/share/userfeedback/scripts/font_info"),
  Log(kFile, "fwupd_state", "/var/lib/fwupd/state.json"),
  Log(kCommand, "sensor_info", "/usr/share/userfeedback/scripts/sensor_info"),
  Log(kFile, "hammerd", "/var/log/hammerd.log"),
  Log(kCommand, "hardware_class", "/usr/bin/crossystem hwid"),
  Log(kCommand, "hostname", "/bin/hostname"),
  Log(kFile, "i915_gem_gtt", "/sys/kernel/debug/dri/0/i915_gem_gtt",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kFile, "i915_gem_objects", "/sys/kernel/debug/dri/0/i915_gem_objects",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kFile, "i915_gem_objects", "/sys/kernel/debug/dri/0/i915_gem_objects",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kCommand, "i915_error_state",
      "/usr/bin/xz -c /sys/kernel/debug/dri/0/i915_error_state 2>/dev/null",
      SandboxedProcess::kDefaultUser, kDebugfsGroup, Log::kDefaultMaxBytes,
      LogTool::Encoding::kBinary),
  Log(kCommand, "ifconfig", "/bin/ifconfig -a"),
  Log(kFile, "input_devices", "/proc/bus/input/devices"),
  // Information about the wiphy device, such as current channel.
  Log(kCommand, "iw_dev", "/usr/sbin/iw dev"),
  // Hardware capabilities of the wiphy device.
  Log(kCommand, "iw_list", "/usr/sbin/iw list"),
#if USE_IWLWIFI_DUMP
  Log(kCommand, "iwlmvm_module_params", CMD_KERNEL_MODULE_PARAMS(iwlmvm)),
  Log(kCommand, "iwlwifi_module_params", CMD_KERNEL_MODULE_PARAMS(iwlwifi)),
#endif  // USE_IWLWIFI_DUMP
  Log(kCommand, "kernel-crashes",
      "cat /var/spool/crash/kernel.*.kcrash 2>/dev/null"),
  Log(kCommand, "logcat", "/usr/sbin/android-sh -c '/system/bin/logcat -d'",
      kRoot, kRoot, Log::kDefaultMaxBytes, LogTool::Encoding::kUtf8),
  Log(kCommand, "lsmod", "lsmod"),
  Log(kCommand, "lspci", "/usr/sbin/lspci"),
  Log(kCommand, "lsusb", "lsusb && lsusb -t"),
  Log(kFile, "mali_memory", "/sys/class/misc/mali0/device/memory"),
  Log(kFile, "memd.parameters", "/var/log/memd/memd.parameters"),
  Log(kCommand, "memd clips", "cat /var/log/memd/memd.clip* 2>/dev/null"),
  Log(kFile, "meminfo", "/proc/meminfo"),
  Log(kCommand, "memory_spd_info",
      // mosys may use 'i2c-dev', which may not be loaded yet.
      "modprobe i2c-dev 2>/dev/null && "
      "mosys -l memory spd print all 2>/dev/null",
      kRoot, kDebugfsGroup),
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
  Log(kCommand, "modetest",
      "(modetest; modetest -M evdi; modetest -M udl) | "
      "sed -E '/EDID/ {:a;n;/value:/!ba;n;"
      "s/(00f{12}00)([0-9a-f]{8})([0-9a-f]{8})/\\1\\200000000/}'",
      kRoot, kRoot),
  Log(kFile, "mount-encrypted", "/var/log/mount-encrypted.log"),
  Log(kFile, "mountinfo", "/proc/1/mountinfo"),
  Log(kCommand, "netlog",
      "/usr/share/userfeedback/scripts/getmsgs /var/log/net.log"),
  // --processes requires root.
  Log(kCommand, "netstat",
      "/sbin/ss --all --query inet --numeric --processes", kRoot, kRoot),
  Log(kFile, "nvmap_iovmm", "/sys/kernel/debug/nvmap/iovmm/allocations",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kCommand, "oemdata", "/usr/share/cros/oemdata.sh", kRoot, kRoot),
  Log(kFile, "pagetypeinfo", "/proc/pagetypeinfo"),
  Log(kCommand, "platform_info",
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
      kRoot, kDebugfsGroup),
  Log(kCommand, "power_supply_info", "/usr/bin/power_supply_info"),
  Log(kCommand, "power_supply_sysfs",
      "/usr/bin/print_sysfs_power_supply_data"),
  Log(kFile, "powerd.LATEST", "/var/log/power_manager/powerd.LATEST"),
  Log(kFile, "powerd.PREVIOUS", "/var/log/power_manager/powerd.PREVIOUS"),
  Log(kFile, "powerd.out", "/var/log/powerd.out"),
  Log(kFile, "powerwash_count", "/var/log/powerwash_count"),
  // Changed from 'ps ux' to 'ps aux' since we're running as debugd,
  // not chronos.
  Log(kCommand, "ps", "/bin/ps aux"),
  // /proc/slabinfo is owned by root and has 0400 permission.
  Log(kFile, "slabinfo", "/proc/slabinfo", kRoot, kRoot),
  Log(kFile, "storage_info", "/var/log/storage_info.txt"),
  Log(kCommand, "swap_info",
      "/usr/share/cros/init/swap.sh status 2>/dev/null",
      SandboxedProcess::kDefaultUser, kDebugfsGroup),
  Log(kCommand, "syslog",
      "/usr/share/userfeedback/scripts/getmsgs /var/log/messages"),
  Log(kCommand, "system_log_stats",
      "echo 'BLOCK_SIZE=1024'; "
      "find /var/log/ -type f -exec du --block-size=1024 {} + | sort -n -r",
      kRoot, kRoot),
  Log(kCommand, "threads",
      "/bin/ps -T axo pid,ppid,spid,pcpu,ni,stat,time,comm"),
  Log(kFile, "tlsdate", "/var/log/tlsdate.log"),
  Log(kCommand, "top thread", "/usr/bin/top -Hb -n 1 | head -n 40"),
  Log(kCommand, "top memory", "/usr/bin/top -o \"+%MEM\" -bn 1 | head -n 57"),
  Log(kCommand, "touch_fw_version",
      "grep -E"
      " -e 'synaptics: Touchpad model'"
      " -e 'chromeos-[a-z]*-touch-[a-z]*-update'"
      " /var/log/messages | tail -n 20"),
  Log(kCommand, "tpm-firmware-updater",
      "/usr/share/userfeedback/scripts/getmsgs "
      "/var/log/tpm-firmware-updater.log"),
  // TODO(jorgelo,mnissler): Don't run this as root.
  // On TPM 1.2 devices this will likely require adding a new user to the 'tss'
  // group.
  // On TPM 2.0 devices 'get_version_info' uses D-Bus and therefore can run as
  // any user.
  Log(kCommand, "tpm_version", "/usr/sbin/tpm-manager get_version_info", kRoot,
      kRoot),
  Log(kCommand, "atmel_ts_refs",
      "/opt/google/touch/scripts/atmel_tools.sh ts r", kRoot, kRoot),
  Log(kCommand, "atmel_tp_refs",
      "/opt/google/touch/scripts/atmel_tools.sh tp r", kRoot, kRoot),
  Log(kCommand, "atmel_ts_deltas",
      "/opt/google/touch/scripts/atmel_tools.sh ts d", kRoot, kRoot),
  Log(kCommand, "atmel_tp_deltas",
      "/opt/google/touch/scripts/atmel_tools.sh tp d", kRoot, kRoot),
  Log(kFile, "stateful_trim_state", "/var/lib/trim/stateful_trim_state"),
  Log(kFile, "stateful_trim_data", "/var/lib/trim/stateful_trim_data"),
  Log(kCommand, "ui_log",
      "/usr/share/userfeedback/scripts/get_log /var/log/ui/ui.LATEST"),
  Log(kCommand, "uname", "/bin/uname -a"),
  Log(kCommand, "update_engine.log",
      "cat $(ls -1tr /var/log/update_engine | tail -5 | sed"
      " s.^./var/log/update_engine/.)"),
  Log(kCommand, "uptime", "/usr/bin/cut -d' ' -f1 /proc/uptime"),
  Log(kFile, "verified boot", "/var/log/debug_vboot_noisy.log"),
  Log(kFile, "vmlog.1.LATEST", "/var/log/vmlog/vmlog.1.LATEST"),
  Log(kFile, "vmlog.1.PREVIOUS", "/var/log/vmlog/vmlog.1.PREVIOUS"),
  Log(kFile, "vmlog.LATEST", "/var/log/vmlog/vmlog.LATEST"),
  Log(kFile, "vmlog.PREVIOUS", "/var/log/vmlog/vmlog.PREVIOUS"),
  Log(kFile, "vmstat", "/proc/vmstat"),
  Log(kFile, "vpd_2.0", "/var/log/vpd_2.0.txt"),
  Log(kCommand, "wifi_status",
      "/usr/bin/network_diag --wifi-internal --no-log"),
  Log(kFile, "zram compressed data size", "/sys/block/zram0/compr_data_size"),
  Log(kFile, "zram original data size", "/sys/block/zram0/orig_data_size"),
  Log(kFile, "zram total memory used", "/sys/block/zram0/mem_used_total"),
  Log(kFile, "zram total reads", "/sys/block/zram0/num_reads"),
  Log(kFile, "zram total writes", "/sys/block/zram0/num_writes"),
  Log(kCommand, "zram new stats names",
      "echo orig_size compr_size used_total limit used_max zero_pages "
      "migrated"),
  Log(kFile, "zram new stats values", "/sys/block/zram0/mm_stat"),
  Log(kFile, "cros_tp version", "/sys/class/chromeos/cros_tp/version"),
  Log(kCommand, "cros_tp console", "/usr/sbin/ectool --name=cros_tp console",
      kRoot, kRoot),
  Log(kCommand, "cros_tp frame", "/usr/sbin/ectool --name=cros_tp tpframeget",
      kRoot, kRoot),
  Log(kCommand, "crostini", "/usr/bin/cicerone_client --get_info"),
  // Stuff pulled out of the original list. These need access to the running X
  // session, which we'd rather not give to debugd, or return info specific to
  // the current session (in the setsid(2) sense), which is not useful for
  // debugd
  // Log(kCommand, "env", "set"),
  // Log(kCommand, "setxkbmap", "/usr/bin/setxkbmap -print -query"),
  // Log(kCommand, "xrandr", "/usr/bin/xrandr --verbose)
};

// netstat and logcat should appear in chrome://system but not in feedback
// reports.  Open sockets may have privacy implications, and logcat is
// already incorporated via arc-bugreport.
const std::vector<string> kCommandLogsExclude = {"netstat", "logcat"};

const std::vector<Log> kExtraLogs {
#if USE_CELLULAR
  Log(kCommand, "mm-status", "/usr/bin/modem status"),
#endif  // USE_CELLULAR
  Log(kCommand, "network-devices", "/usr/bin/connectivity show devices"),
  Log(kCommand, "network-services", "/usr/bin/connectivity show services"),
};

const std::vector<Log> kFeedbackLogs {
#if USE_CELLULAR
  Log(kCommand, "mm-status", "/usr/bin/modem status-feedback"),
#endif  // USE_CELLULAR
  Log(kCommand, "network-devices",
      "/usr/bin/connectivity show-feedback devices"),
  Log(kCommand, "network-services",
          "/usr/bin/connectivity show-feedback services"),
};

// List of log files needed to be part of the feedback report that are huge and
// must be sent back to the client via the file descriptor using
// LogTool::GetBigFeedbackLogs().
const std::vector<Log> kBigFeedbackLogs{
  // ARC bugreport permissions are weird. Since we're just reading a file,
  // this shouldn't cause any issues.
  Log(kFile, "arc-bugreport", "/run/arc/bugreport/pipe", kRoot, kRoot,
      10 * 1024 * 1024, LogTool::Encoding::kUtf8),
};

// List of log files that must directly be collected by Chrome. This is because
// debugd is running under a VFS namespace and does not have access to later
// cryptohome mounts.
using UserLog = std::pair<std::string, std::string>;
const std::vector<UserLog> kUserLogs = {
  UserLog("chrome_user_log", "log/chrome"),
  UserLog("libassistant_user_log", "log/libassistant.log"),
  UserLog("login-times", "login-times"),
  UserLog("logout-times", "logout-times"),
};

// Fills |dictionary| with the anonymized contents of the logs in |logs|.
void GetLogsInDictionary(const std::vector<Log>& logs,
                         AnonymizerTool* anonymizer,
                         base::DictionaryValue* dictionary) {
  for (const Log& log : logs) {
    dictionary->SetStringWithoutPathExpansion(
        log.GetName(), anonymizer->Anonymize(log.GetLogData()));
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

bool GetNamedLogFrom(const string& name,
                     const std::vector<Log>& logs,
                     string* result) {
  for (const Log& log : logs) {
    if (name == log.GetName()) {
      *result = log.GetLogData();
      return true;
    }
  }
  *result = "<invalid log name>";
  return false;
}

void GetLogsFrom(const std::vector<Log>& logs, LogTool::LogMap* map) {
  for (const Log& log : logs)
    (*map)[log.GetName()] = log.GetLogData();
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
      std::string(kPerfDataDescription) +
      LogTool::EnsureUTF8String(perf_data_str, LogTool::Encoding::kBinary);
}

}  // namespace

Log::Log(Log::LogType type,
         std::string name,
         std::string data,
         std::string user,
         std::string group,
         int64_t max_bytes,
         LogTool::Encoding encoding)
    : type_(type),
      name_(name),
      data_(data),
      user_(user),
      group_(group),
      max_bytes_(max_bytes),
      encoding_(encoding) {}

std::string Log::GetName() const {
  return name_;
}

std::string Log::GetLogData() const {
  // The reason this code uses a switch statement on a type enum rather than
  // using inheritance/virtual dispatch is so that all of the Log objects can
  // be constructed statically. Switching to heap allocated subclasses of Log
  // makes the code that declares all of the log entries much more verbose
  // and harder to understand.
  switch (type_) {
    case kCommand:
      return GetCommandLogData();
    case kFile:
      return GetFileLogData();
    default:
      return "<unknown log type>";
  }
}

// TODO(ellyjones): sandbox. crosbug.com/35122
std::string Log::GetCommandLogData() const {
  if (type_ != kCommand)
    return "<log type mismatch>";
  std::string tailed_cmdline =
      base::StringPrintf("%s | tail -c %" PRId64, data_.c_str(), max_bytes_);
  ProcessWithOutput p;
  if (minijail_disabled_for_test_)
    p.set_use_minijail(false);
  if (!user_.empty() && !group_.empty())
    p.SandboxAs(user_, group_);
  if (!p.Init())
    return "<not available>";
  p.AddArg(kShell);
  p.AddStringOption("-c", tailed_cmdline);
  if (p.Run())
    return "<not available>";
  std::string output;
  p.GetOutput(&output);
  if (output.empty())
    return "<empty>";
  return LogTool::EnsureUTF8String(output, encoding_);
}

std::string Log::GetFileLogData() const {
  if (type_ != kFile)
    return "<log type mismatch>";

  uid_t old_euid = geteuid();
  uid_t new_euid = UidForUser(user_);
  gid_t old_egid = getegid();
  gid_t new_egid = GidForGroup(group_);

  if (new_euid == -1 || new_egid == -1) {
    return "<not available>";
  }

  // Make sure to set group first, since if we set user first we lose root
  // and therefore the ability to set our effective gid to arbitrary gids.
  if (setegid(new_egid)) {
    PLOG(ERROR) << "Failed to set effective group id to " << new_egid;
    return "<not available>";
  }
  if (seteuid(new_euid)) {
    PLOG(ERROR) << "Failed to set effective user id to " << new_euid;
    if (setegid(old_egid))
      PLOG(ERROR) << "Failed to restore effective group id to " << old_egid;
    return "<not available>";
  }

  std::string contents;
  const base::FilePath path(data_);
  // Handle special files that don't properly report length/allow lseek.
  if (base::FilePath("/dev").IsParent(path) ||
      base::FilePath("/proc").IsParent(path) ||
      base::FilePath("/sys").IsParent(path)) {
    if (!base::ReadFileToString(path, &contents))
      contents = "<not available>";
    if (contents.size() > max_bytes_)
      contents.erase(0, contents.size() - max_bytes_);
  } else {
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      contents = "<not available>";
    } else {
      int64_t length = file.GetLength();
      if (length > max_bytes_) {
        file.Seek(base::File::FROM_END, -max_bytes_);
        length = max_bytes_;
      }
      std::vector<char> buf(length);
      int read = file.ReadAtCurrentPos(buf.data(), buf.size());
      if (read < 0) {
        PLOG(ERROR) << "Could not read from file " << path.value();
      } else {
        contents = std::string(buf.begin(), buf.begin() + read);
      }
    }
  }

  if (contents.empty())
    contents = "<empty>";

  // Make sure we restore our old euid/egid before returning.
  if (seteuid(old_euid))
    PLOG(ERROR) << "Failed to restore effective user id to " << old_euid;

  if (setegid(old_egid))
    PLOG(ERROR) << "Failed to restore effective group id to " << old_egid;

  return LogTool::EnsureUTF8String(contents, encoding_);
}

void Log::DisableMinijailForTest() {
  minijail_disabled_for_test_ = true;
}

// static
uid_t Log::UidForUser(const std::string& user) {
  struct passwd entry;
  struct passwd* result;
  std::vector<char> buf(1024);
  getpwnam_r(user.c_str(), &entry, &buf[0], buf.size(), &result);
  if (!result) {
    LOG(ERROR) << "User not found: " << user;
    return -1;
  }
  return entry.pw_uid;
}

// static
gid_t Log::GidForGroup(const std::string& group) {
  struct group entry;
  struct group* result;
  std::vector<char> buf(1024);
  getgrnam_r(group.c_str(), &entry, &buf[0], buf.size(), &result);
  if (!result) {
    LOG(ERROR) << "Group not found: " << group;
    return -1;
  }
  return entry.gr_gid;
}

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
  for (const UserLog& ul : kUserLogs)
    result[ul.first] = ul.second;
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
