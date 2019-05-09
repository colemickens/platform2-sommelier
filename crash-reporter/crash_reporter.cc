// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>  // for open

#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <libminijail.h>
#include <metrics/metrics_library.h>

#include "crash-reporter/arc_collector.h"
#include "crash-reporter/arc_service_failure_collector.h"
#include "crash-reporter/bert_collector.h"
#include "crash-reporter/chrome_collector.h"
#include "crash-reporter/early_crash_meta_collector.h"
#include "crash-reporter/ec_collector.h"
#include "crash-reporter/kernel_collector.h"
#include "crash-reporter/kernel_warning_collector.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/selinux_violation_collector.h"
#include "crash-reporter/service_failure_collector.h"
#include "crash-reporter/udev_collector.h"
#include "crash-reporter/unclean_shutdown_collector.h"
#include "crash-reporter/user_collector.h"

using base::FilePath;

namespace {

const char kKernelCrashDetected[] = "/run/kernel-crash-detected";
const char kUncleanShutdownDetected[] = "/run/unclean-shutdown-detected";

MetricsLibrary s_metrics_lib;

bool IsFeedbackAllowed() {
  return s_metrics_lib.AreMetricsEnabled();
}

bool TouchFile(const FilePath& file_path) {
  return base::WriteFile(file_path, "", 0) == 0;
}

int Initialize(UserCollector* user_collector,
               UdevCollector* udev_collector,
               bool early) {
  // Set up all the common crash state directories first.  If we can't guarantee
  // these basic paths, just give up & don't turn on anything else.
  if (!CrashCollector::InitializeSystemCrashDirectories(early))
    return 1;

  int ret = 0;

  if (!user_collector->Enable(early))
    ret = 1;
  if (!udev_collector->Enable())
    ret = 1;
  return ret;
}

int BootCollect(KernelCollector* kernel_collector,
                ECCollector* ec_collector,
                BERTCollector* bert_collector,
                UncleanShutdownCollector* unclean_shutdown_collector,
                EarlyCrashMetaCollector* early_crash_meta_collector) {
  bool was_kernel_crash = false;
  bool was_unclean_shutdown = false;

  /* TODO(drinkcat): Distinguish between EC crash and unclean shutdown. */
  ec_collector->Collect();

  // Invoke to collect firmware bert dump.
  bert_collector->Collect();

  kernel_collector->Enable();
  if (kernel_collector->is_enabled()) {
    was_kernel_crash = kernel_collector->Collect();
  }
  was_unclean_shutdown = unclean_shutdown_collector->Collect();

  // Touch a file to notify the metrics daemon that a kernel
  // crash has been detected so that it can log the time since
  // the last kernel crash.
  if (IsFeedbackAllowed()) {
    if (was_kernel_crash) {
      TouchFile(FilePath(kKernelCrashDetected));
    } else if (was_unclean_shutdown) {
      // We only count an unclean shutdown if it did not come with
      // an associated kernel crash.
      TouchFile(FilePath(kUncleanShutdownDetected));
    }
  }

  // Must enable the unclean shutdown collector *after* collecting.
  unclean_shutdown_collector->Enable();

  // Copy lsb-release and os-release into system crash spool.  Done after
  // collecting so that boot-time collected crashes will be associated with the
  // previous boot.
  unclean_shutdown_collector->SaveVersionData();

  // Collect early boot crashes.
  early_crash_meta_collector->Collect();

  return 0;
}

int HandleUserCrash(UserCollector* user_collector,
                    const std::string& user,
                    const bool crash_test,
                    const bool early) {
  // Handle a specific user space crash.
  CHECK(!user.empty()) << "--user= must be set";

  // Make it possible to test what happens when we crash while
  // handling a crash.
  if (crash_test) {
    *(volatile char*)0 = 0;
    return 0;
  }

  // Accumulate logs to help in diagnosing failures during user collection.
  brillo::LogToString(true);
  // Handle the crash, get the name of the process from procfs.
  bool handled = user_collector->HandleCrash(user, nullptr);
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}

#if USE_CHEETS
int HandleArcCrash(ArcCollector* arc_collector, const std::string& user) {
  brillo::LogToString(true);
  bool handled = arc_collector->HandleCrash(user, nullptr);
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}

int HandleArcJavaCrash(ArcCollector* arc_collector,
                       const std::string& crash_type,
                       const std::string& device,
                       const std::string& board,
                       const std::string& cpu_abi) {
  brillo::LogToString(true);
  bool handled =
      arc_collector->HandleJavaCrash(crash_type, device, board, cpu_abi);
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}
#endif

int HandleChromeCrash(ChromeCollector* chrome_collector,
                      const std::string& chrome_dump_file,
                      pid_t pid,
                      uid_t uid,
                      const std::string& exe) {
  CHECK(!chrome_dump_file.empty()) << "--chrome= must be set";
  CHECK(pid != (pid_t)-1) << "--pid= must be set";
  CHECK(uid != (uid_t)-1) << "--uid= must be set";
  CHECK(!exe.empty()) << "--exe= must be set";

  brillo::LogToString(true);
  bool handled =
      chrome_collector->HandleCrash(FilePath(chrome_dump_file), pid, uid, exe);
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}

int HandleUdevCrash(UdevCollector* udev_collector,
                    const std::string& udev_event) {
  // Handle a crash indicated by a udev event.
  CHECK(!udev_event.empty()) << "--udev= must be set";

  // Accumulate logs to help in diagnosing failures during user collection.
  brillo::LogToString(true);
  bool handled = udev_collector->HandleCrash(udev_event);
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}

int HandleKernelWarning(KernelWarningCollector* kernel_warning_collector,
                        KernelWarningCollector::WarningType type) {
  // Accumulate logs to help in diagnosing failures during collection.
  brillo::LogToString(true);
  bool handled = kernel_warning_collector->Collect(type);
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}

int HandleServiceFailure(ServiceFailureCollector* service_failure_collector,
                         const std::string& service_name) {
  // Accumulate logs to help in diagnosing failures during collection.
  brillo::LogToString(true);
  service_failure_collector->SetServiceName(service_name);
  bool handled = service_failure_collector->Collect();
  brillo::LogToString(false);
  if (!handled)
    return 1;
  return 0;
}

int HandleSELinuxViolation(
    SELinuxViolationCollector* selinux_violation_collector) {
  brillo::LogToString(true);
  bool handled = selinux_violation_collector->Collect();
  brillo::LogToString(false);
  return handled ? 0 : 1;
}

// Ensure stdout, stdin, and stderr are open file descriptors.  If
// they are not, any code which writes to stderr/stdout may write out
// to files opened during execution.  In particular, when
// crash_reporter is run by the kernel coredump pipe handler (via
// kthread_create/kernel_execve), it will not have file table entries
// 1 and 2 (stdout and stderr) populated.  We populate them here.
void OpenStandardFileDescriptors() {
  int new_fd = -1;
  // We open /dev/null to fill in any of the standard [0, 2] file
  // descriptors.  We leave these open for the duration of the
  // process.  This works because open returns the lowest numbered
  // invalid fd.
  do {
    new_fd = open("/dev/null", 0);
    CHECK_GE(new_fd, 0) << "Unable to open /dev/null";
  } while (new_fd >= 0 && new_fd <= 2);
  close(new_fd);
}

// Reduce privs that we don't need.  But we still need:
// - The top most /proc to pull details out of it.
// - Read access to the crashing process's memory (regardless of user).
// - Write access to the crash spool dir.
void EnterSandbox(bool write_proc, bool log_to_stderr) {
  // If we're not root, we won't be able to jail ourselves (well, we could if
  // we used user namespaces, but maybe later).  Need to double check handling
  // when called by chrome to process its crashes.
  if (getuid() != 0)
    return;

  struct minijail* j = minijail_new();
  minijail_namespace_ipc(j);
  minijail_namespace_uts(j);
  minijail_namespace_net(j);
  minijail_namespace_vfs(j);
  minijail_mount_tmp(j);
  minijail_mount_dev(j);
  if (!log_to_stderr)
    minijail_bind(j, "/dev/log", "/dev/log", 0);
  minijail_no_new_privs(j);
  minijail_new_session_keyring(j);

  // If we're initializing the system, we're need to write to /proc/sys/.
  if (!write_proc) {
    minijail_remount_proc_readonly(j);
  }

  minijail_enter(j);
  minijail_destroy(j);
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_bool(init, false, "Initialize crash logging");
  DEFINE_bool(boot_collect, false, "Run per-boot crash collection tasks");
  DEFINE_bool(clean_shutdown, false, "Signal clean shutdown");
  DEFINE_bool(crash_test, false, "Crash test");
  DEFINE_bool(early, false,
              "Modifies crash-reporter to work during early boot");
  DEFINE_bool(preserve_across_clobber, false,
              "Persist early user crash reports across clobbers.");
  DEFINE_string(user, "", "User crash info (pid:signal:exec_name)");
  DEFINE_string(udev, "", "Udev event description (type:device:subsystem)");
  DEFINE_bool(kernel_warning, false, "Report collected kernel warning");
  DEFINE_bool(kernel_wifi_warning, false,
              "Report collected kernel wifi warning");
  DEFINE_bool(kernel_suspend_warning, false,
              "Report collected kernel suspend warning");
  DEFINE_bool(log_to_stderr, false, "Log to stderr instead of syslog.");
  DEFINE_string(arc_service_failure, "",
                "The specific ARC service name that failed");
  DEFINE_string(service_failure, "", "The specific service name that failed");
  DEFINE_bool(selinux_violation, false, "Report collected SELinux violation");
  DEFINE_string(chrome, "", "Chrome crash dump file");
  DEFINE_int32(pid, -1, "PID of crashing process");
  DEFINE_int32(uid, -1, "UID of crashing process");
  DEFINE_string(exe, "", "Executable name of crashing process");
  DEFINE_int64(crash_loop_before, -1,
               "UNIX timestamp. If invoked before this time, use the special "
               "login-crash-loop handling system. (Keep crash report in memory "
               "and then pass to debugd for immediate upload.)")
      DEFINE_bool(core2md_failure, false, "Core2md failure test");
  DEFINE_bool(directory_failure, false, "Spool directory failure test");
  DEFINE_string(filter_in, "", "Ignore all crashes but this for testing");
#if USE_CHEETS
  DEFINE_string(arc_java_crash, "",
                "Read Java crash log of the given type from standard input");
  DEFINE_string(arc_device, "", "Metadata for --arc_java_crash");
  DEFINE_string(arc_board, "", "Metadata for --arc_java_crash");
  DEFINE_string(arc_cpu_abi, "", "Metadata for --arc_java_crash");
#endif

  OpenStandardFileDescriptors();
  FilePath my_path = base::MakeAbsoluteFilePath(FilePath(argv[0]));
  brillo::FlagHelper::Init(argc, argv, "Chromium OS Crash Reporter");

  base::MessageLoopForIO message_loop;

  // In certain cases, /dev/log may not be available: log to stderr instead.
  if (FLAGS_log_to_stderr) {
    brillo::InitLog(brillo::kLogToStderr);
  } else {
    brillo::OpenLog(my_path.BaseName().value().c_str(), true);
    brillo::InitLog(brillo::kLogToSyslog);
  }

  // Now that we've processed the command line, sandbox ourselves.
  EnterSandbox(FLAGS_init || FLAGS_clean_shutdown, FLAGS_log_to_stderr);

  EarlyCrashMetaCollector early_crash_meta_collector;
  early_crash_meta_collector.Initialize(IsFeedbackAllowed,
                                        FLAGS_preserve_across_clobber);

  // Decide if we should use Crash-Loop sending mode. If session_manager sees
  // several Chrome crashes in a brief period, it will log the user out. On the
  // last Chrome startup before it logs the user out, it will set the
  // --crash_loop_before flag. The value of the flag will be a time_t timestamp
  // giving the last second at which a crash would be considered a crash loop
  // and thus log the user out. If we have another crash before that second,
  // we have detected a crash-loop and we want to invoke special handling
  // (specifically, we don't want to save the crash in the user's home directory
  // because that will be inaccessible to crash_sender once the user is logged
  // out).
  CrashCollector::CrashSendingMode crash_sending_mode =
      CrashCollector::kNormalCrashSendMode;
  if (FLAGS_crash_loop_before >= 0) {
    base::Time crash_loop_before =
        base::Time::FromTimeT(static_cast<time_t>(FLAGS_crash_loop_before));
    if (base::Time::Now() <= crash_loop_before) {
      crash_sending_mode = CrashCollector::kCrashLoopSendingMode;
    }
  }

  KernelCollector kernel_collector;
  kernel_collector.Initialize(IsFeedbackAllowed, FLAGS_early);
  ECCollector ec_collector;
  ec_collector.Initialize(IsFeedbackAllowed, FLAGS_early);
  BERTCollector bert_collector;
  bert_collector.Initialize(IsFeedbackAllowed, FLAGS_early);
  UserCollector user_collector;
  UserCollector::FilterOutFunction filter_out = [](pid_t) { return false; };
#if USE_CHEETS
  ArcCollector arc_collector;
  arc_collector.Initialize(IsFeedbackAllowed,
                           true,  // generate_diagnostics
                           FLAGS_directory_failure, FLAGS_filter_in,
                           false /* early */);
  // Filter out ARC processes.
  if (ArcCollector::IsArcRunning())
    filter_out = std::bind(&ArcCollector::IsArcProcess, &arc_collector,
                           std::placeholders::_1);
#endif
  user_collector.Initialize(my_path.value(), IsFeedbackAllowed,
                            true,  // generate_diagnostics
                            FLAGS_core2md_failure, FLAGS_directory_failure,
                            FLAGS_filter_in, std::move(filter_out),
                            FLAGS_early);
  UncleanShutdownCollector unclean_shutdown_collector;
  unclean_shutdown_collector.Initialize(IsFeedbackAllowed, FLAGS_early);

  UdevCollector udev_collector;
  udev_collector.Initialize(IsFeedbackAllowed, FLAGS_early);
  ChromeCollector chrome_collector(crash_sending_mode);
  chrome_collector.Initialize(IsFeedbackAllowed, FLAGS_early);

  KernelWarningCollector kernel_warning_collector;
  kernel_warning_collector.Initialize(IsFeedbackAllowed, FLAGS_early);

  ArcServiceFailureCollector arc_service_failure_collector;
  arc_service_failure_collector.Initialize(IsFeedbackAllowed, FLAGS_early);

  ServiceFailureCollector service_failure_collector;
  service_failure_collector.Initialize(IsFeedbackAllowed, FLAGS_early);

  SELinuxViolationCollector selinux_violation_collector;
  selinux_violation_collector.Initialize(IsFeedbackAllowed, FLAGS_early);

  if (FLAGS_init) {
    return Initialize(&user_collector, &udev_collector, FLAGS_early);
  }

  if (FLAGS_boot_collect) {
    return BootCollect(&kernel_collector, &ec_collector, &bert_collector,
                       &unclean_shutdown_collector,
                       &early_crash_meta_collector);
  }

  if (FLAGS_clean_shutdown) {
    int ret = 0;
    if (!unclean_shutdown_collector.Disable())
      ret = 1;
    if (!user_collector.Disable())
      ret = 1;
    return ret;
  }

  if (!FLAGS_udev.empty()) {
    return HandleUdevCrash(&udev_collector, FLAGS_udev);
  }

  if (FLAGS_kernel_warning) {
    return HandleKernelWarning(&kernel_warning_collector,
                               KernelWarningCollector::WarningType::kGeneric);
  }

  if (FLAGS_kernel_wifi_warning) {
    return HandleKernelWarning(&kernel_warning_collector,
                               KernelWarningCollector::WarningType::kWifi);
  }

  if (FLAGS_kernel_suspend_warning) {
    return HandleKernelWarning(&kernel_warning_collector,
                               KernelWarningCollector::WarningType::kSuspend);
  }

  if (!FLAGS_arc_service_failure.empty()) {
    return HandleServiceFailure(&arc_service_failure_collector,
                                FLAGS_arc_service_failure);
  }

  if (!FLAGS_service_failure.empty()) {
    return HandleServiceFailure(&service_failure_collector,
                                FLAGS_service_failure);
  }

  if (FLAGS_selinux_violation) {
    return HandleSELinuxViolation(&selinux_violation_collector);
  }

  if (!FLAGS_chrome.empty()) {
    return HandleChromeCrash(&chrome_collector, FLAGS_chrome, FLAGS_pid,
                             FLAGS_uid, FLAGS_exe);
  }

#if USE_CHEETS
  if (!FLAGS_arc_java_crash.empty())
    return HandleArcJavaCrash(&arc_collector, FLAGS_arc_java_crash,
                              FLAGS_arc_device, FLAGS_arc_board,
                              FLAGS_arc_cpu_abi);
#endif

  int exit_code = HandleUserCrash(&user_collector, FLAGS_user, FLAGS_crash_test,
                                  FLAGS_early);
#if USE_CHEETS
  if (ArcCollector::IsArcRunning())
    exit_code |= HandleArcCrash(&arc_collector, FLAGS_user);
#endif
  return exit_code;
}
