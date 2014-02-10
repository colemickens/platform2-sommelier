// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <grp.h>
#include <secder.h>
#include <signal.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <utility>
#include <vector>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/callback.h>
#include <base/command_line.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/message_loop.h>
#include <base/message_loop_proxy.h>
#include <base/posix/eintr_wrapper.h>
#include <base/run_loop.h>
#include <base/stl_util.h>
#include <base/string_util.h>
#include <base/time.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/switches/chrome_switches.h>
#include <chromeos/utility.h>

#include "login_manager/browser_job.h"
#include "login_manager/child_exit_handler.h"
#include "login_manager/dbus_glib_shim.h"
#include "login_manager/key_generator.h"
#include "login_manager/liveness_checker_impl.h"
#include "login_manager/login_metrics.h"
#include "login_manager/nss_util.h"
#include "login_manager/session_manager_impl.h"
#include "login_manager/system_utils.h"
#include "login_manager/termination_handler.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace login_manager {  // NOLINT
namespace gobject {  // NOLINT
#include "login_manager/server.h"
}  // namespace gobject
}  // namespace login_manager

namespace em = enterprise_management;
namespace login_manager {

namespace {
// I need a do-nothing action for SIGALRM, or using alarm() will kill me.
void DoNothing(int signal) {}

}  // anonymous namespace

// TODO(mkrebs): Remove CollectChrome timeout and file when
// crosbug.com/5872 is fixed.
// When crash-reporter based crash reporting of Chrome is enabled
// (which should only be during test runs) we use
// kKillTimeoutCollectChrome instead of the kill timeout specified at
// the command line.
const int SessionManagerService::kKillTimeoutCollectChrome = 60;
const char SessionManagerService::kCollectChromeFile[] =
    "/mnt/stateful_partition/etc/collect_chrome_crashes";

void SessionManagerService::TestApi::ScheduleChildExit(pid_t pid, int status) {
  siginfo_t info;
  info.si_pid = pid;
  if (WIFEXITED(status)) {
    info.si_code = CLD_EXITED;
    info.si_status = WEXITSTATUS(status);
  } else {
    info.si_status = WTERMSIG(status);
  }
  session_manager_service_->loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&SessionManagerService::HandleExit,
                 session_manager_service_, info));
}

SessionManagerService::SessionManagerService(
    scoped_ptr<BrowserJobInterface> child_job,
    const base::Closure& quit_closure,
    uid_t uid,
    int kill_timeout,
    bool enable_browser_abort_on_hang,
    base::TimeDelta hang_detection_interval,
    LoginMetrics* metrics,
    SystemUtils* utils)
    : browser_(child_job.Pass()),
      exit_on_child_done_(false),
      kill_timeout_(base::TimeDelta::FromSeconds(kill_timeout)),
      session_manager_(NULL),
      main_loop_(NULL),
      loop_proxy_(MessageLoop::current()->message_loop_proxy()),
      quit_closure_(quit_closure),
      login_metrics_(metrics),
      system_(utils),
      nss_(NssUtil::Create()),
      key_gen_(uid, utils),
      liveness_checker_(NULL),
      enable_browser_abort_on_hang_(enable_browser_abort_on_hang),
      liveness_checking_interval_(hang_detection_interval),
      child_exit_handler_(utils),
      ALLOW_THIS_IN_INITIALIZER_LIST(term_handler_(this)),
      shutting_down_(false),
      shutdown_already_(false),
      exit_code_(SUCCESS) {
  SetUpHandlers();
}

SessionManagerService::~SessionManagerService() {
  if (main_loop_)
    g_main_loop_unref(main_loop_);
  if (session_manager_)
    g_object_unref(session_manager_);

  // Remove this in case it was added by StopSession().
  g_idle_remove_by_data(this);
  RevertHandlers();
}

bool SessionManagerService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(
      gobject::session_manager_get_type(),
      &gobject::dbus_glib_session_manager_object_info);
  dbus_glib_global_set_disable_legacy_property_access();
  // Register DBus signals with glib.
  // The gobject system seems to need to have glib signals hanging off
  // the SessionManager gtype that correspond with the DBus signals we
  // wish to emit.  These calls create and register them.
  // TODO(cmasone): remove these when we migrate away from dbus-glib.
  g_signal_new("session_state_changed",
               gobject::session_manager_get_type(),
               G_SIGNAL_RUN_LAST,
               0,               // class offset
               NULL, NULL,      // accumulator and data
               // TODO: This is wrong.  If you need to use it at some point
               // (probably only if you need to listen to this GLib signal
               // instead of to the D-Bus signal), you should generate an
               // appropriate marshaller that takes two string arguments.
               // See e.g. http://goo.gl/vEGT4.
               g_cclosure_marshal_VOID__STRING,
               G_TYPE_NONE,     // return type
               2,               // num params
               G_TYPE_STRING,   // "started", "stopping", or "stopped"
               G_TYPE_STRING);  // current user
  g_signal_new("login_prompt_visible",
               gobject::session_manager_get_type(),
               G_SIGNAL_RUN_LAST,
               0,               // class offset
               NULL, NULL,      // accumulator and data
               NULL,            // Use default marshaller.
               G_TYPE_NONE,     // return type
               0);              // num params
  g_signal_new("screen_is_locked",
               gobject::session_manager_get_type(),
               G_SIGNAL_RUN_LAST,
               0,               // class offset
               NULL, NULL,      // accumulator and data
               NULL,            // Use default marshaller.
               G_TYPE_NONE,     // return type
               0);              // num params
  g_signal_new("screen_is_unlocked",
               gobject::session_manager_get_type(),
               G_SIGNAL_RUN_LAST,
               0,               // class offset
               NULL, NULL,      // accumulator and data
               NULL,            // Use default marshaller.
               G_TYPE_NONE,     // return type
               0);              // num params

  LOG(INFO) << "SessionManagerService starting";

  if (!Reset())
    return false;

  // Initially store in derived-type pointer, so that we can initialize
  // appropriately below..
  SessionManagerImpl* impl =
      new SessionManagerImpl(
          scoped_ptr<UpstartSignalEmitter>(new UpstartSignalEmitter),
          &dbus_emitter_, &key_gen_, this, login_metrics_, nss_.get(), system_);

  // Wire impl to dbus-glib glue.
  if (session_manager_)
    g_object_unref(session_manager_);
  session_manager_ =
      reinterpret_cast<gobject::SessionManager*>(
          g_object_new(gobject::session_manager_get_type(), NULL));
  session_manager_->impl = impl;

  liveness_checker_.reset(
      new LivenessCheckerImpl(this,
                              system_,
                              loop_proxy_,
                              enable_browser_abort_on_hang_,
                              liveness_checking_interval_));

  impl_.reset(impl);
  if (!InitializeImpl())
    return false;

  // Set any flags that were specified system-wide.
  browser_->SetExtraArguments(impl_->GetStartUpFlags());

  return true;
}

bool SessionManagerService::Register(
    const chromeos::dbus::BusConnection &connection) {
  if (!chromeos::dbus::AbstractDbusService::Register(connection))
    return false;
  const std::string filter =
      StringPrintf("type='method_call', interface='%s'", service_interface());
  DBusConnection* conn =
      ::dbus_g_connection_get_connection(connection.g_connection());
  CHECK(conn);
  DBusError error;
  ::dbus_error_init(&error);
  ::dbus_bus_add_match(conn, filter.c_str(), &error);
  if (::dbus_error_is_set(&error)) {
    LOG(WARNING) << "Failed to add match to bus: " << error.name << ", message="
                 << (error.message ? error.message : "unknown error");
    return false;
  }
  if (!::dbus_connection_add_filter(conn,
                                    &SessionManagerService::FilterMessage,
                                    this,
                                    NULL)) {
    LOG(WARNING) << "Failed to add filter to connection";
    return false;
  }
  return true;
}

bool SessionManagerService::Reset() {
  if (main_loop_)
    g_main_loop_unref(main_loop_);
  main_loop_ = g_main_loop_new(NULL, false);
  if (!main_loop_) {
    LOG(ERROR) << "Failed to create main loop";
    return false;
  }
  return true;
}

bool SessionManagerService::Run() {
  NOTREACHED();
  return false;
}

bool SessionManagerService::Shutdown() {
  NOTREACHED();
  return false;
}

void SessionManagerService::Finalize() {
  LOG(INFO) << "SessionManagerService exiting";
  liveness_checker_->Stop();
  CleanupChildren(GetKillTimeout());
  impl_->Finalize();
  impl_->AnnounceSessionStopped();
}

void SessionManagerService::ScheduleShutdown() {
  SetExitAndScheduleShutdown(SUCCESS);
}

void SessionManagerService::RunBrowser() {
  browser_->RunInBackground();
  DLOG(INFO) << "Browser is " << browser_->CurrentPid();
  liveness_checker_->Start();
}

void SessionManagerService::AbortBrowser(int signal,
                                         const std::string& message) {
  browser_->Kill(signal, message);
  browser_->WaitAndAbort(GetKillTimeout());
}

void SessionManagerService::RestartBrowserWithArgs(
    const std::vector<std::string>& args, bool args_are_extra) {
  // Waiting for Chrome to shutdown takes too much time.
  // We're killing it immediately hoping that data Chrome uses before
  // logging in is not corrupted.
  // TODO(avayvod): Remove RestartJob when crosbug.com/6924 is fixed.
  browser_->KillEverything(SIGKILL, "Restarting browser on-demand.");
  if (args_are_extra)
    browser_->SetExtraArguments(args);
  else
    browser_->SetArguments(args);
  // The browser will be restarted in HandleBrowserExit().
}

void SessionManagerService::SetBrowserSessionForUser(
    const std::string& username,
    const std::string& userhash) {
  browser_->StartSession(username, userhash);
}

void SessionManagerService::SetFlagsForUser(
    const std::string& username,
    const std::vector<std::string>& flags) {
  browser_->SetExtraArguments(flags);
}

bool SessionManagerService::IsBrowser(pid_t pid) {
  return (browser_ &&
          browser_->CurrentPid() > 0 &&
          pid == browser_->CurrentPid());
}

bool SessionManagerService::IsManagedJob(pid_t pid) {
  return IsBrowser(pid);
}

void SessionManagerService::HandleExit(const siginfo_t& ignored) {
  LOG(INFO) << "Exiting process is " << browser_->GetName() << ".";

  // If I could wait for descendants here, I would.  Instead, I kill them.
  browser_->KillEverything(SIGKILL, "Session termination");
  browser_->ClearPid();

  // Do nothing if already shutting down.
  if (shutting_down_)
    return;

  liveness_checker_->Stop();

  if (impl_->ScreenIsLocked()) {
    LOG(ERROR) << "Screen locked, shutting down";
    SetExitAndScheduleShutdown(CRASH_WHILE_SCREEN_LOCKED);
    return;
  }

  DCHECK(browser_.get());
  if (browser_->ShouldStop()) {
    LOG(WARNING) << "Child stopped, shutting down";
    SetExitAndScheduleShutdown(CHILD_EXITING_TOO_FAST);
  } else if (browser_->ShouldRunBrowser()) {
    // TODO(cmasone): deal with fork failing in RunBrowser()
    RunBrowser();
  } else {
    LOG(INFO) << "Should NOT run " << browser_->GetName() << " again.";
    AllowGracefulExitOrRunForever();
  }
}

void SessionManagerService::RequestJobExit() {
  if (browser_ && browser_->CurrentPid() > 0)
    browser_->Kill(SIGTERM, "");
}

void SessionManagerService::EnsureJobExit(base::TimeDelta timeout) {
  if (browser_ && browser_->CurrentPid() > 0)
    browser_->WaitAndAbort(timeout);
}

DBusHandlerResult SessionManagerService::FilterMessage(DBusConnection* conn,
                                                       DBusMessage* message,
                                                       void* data) {
  SessionManagerService* service = static_cast<SessionManagerService*>(data);
  if (::dbus_message_is_method_call(message,
                                    service->service_interface(),
                                    kSessionManagerRestartJob)) {
    const char* sender = ::dbus_message_get_sender(message);
    if (!sender) {
      LOG(ERROR) << "Call to RestartJob has no sender";
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    LOG(INFO) << "Received RestartJob from " << sender;
    DBusMessage* get_pid =
        ::dbus_message_new_method_call("org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       "GetConnectionUnixProcessID");
    CHECK(get_pid);
    ::dbus_message_append_args(get_pid,
                               DBUS_TYPE_STRING, &sender,
                               DBUS_TYPE_INVALID);
    DBusMessage* got_pid =
        ::dbus_connection_send_with_reply_and_block(conn, get_pid, -1, NULL);
    ::dbus_message_unref(get_pid);
    if (!got_pid) {
      LOG(ERROR) << "Could not look up sender of RestartJob";
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    uint32 pid;
    if (!::dbus_message_get_args(got_pid, NULL,
                                 DBUS_TYPE_UINT32, &pid,
                                 DBUS_TYPE_INVALID)) {
      ::dbus_message_unref(got_pid);
      LOG(ERROR) << "Could not extract pid of sender of RestartJob";
      return DBUS_HANDLER_RESULT_HANDLED;
    }
    ::dbus_message_unref(got_pid);
    if (!service->IsBrowser(pid)) {
      LOG(WARNING) << "Sender of RestartJob is no child of mine!";
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void SessionManagerService::SetUpHandlers() {
  // I have to ignore SIGUSR1, because Xorg sends it to this process when it's
  // got no clients and is ready for new ones.  If we don't ignore it, we die.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);

  action.sa_handler = DoNothing;
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);

  std::vector<JobManagerInterface*> job_managers;
  job_managers.push_back(this);
  job_managers.push_back(&key_gen_);
  child_exit_handler_.Init(job_managers);
  term_handler_.Init();
}

void SessionManagerService::RevertHandlers() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);
  ChildExitHandler::RevertHandlers();
  TerminationHandler::RevertHandlers();
}

base::TimeDelta SessionManagerService::GetKillTimeout() {
  if (file_util::PathExists(FilePath(kCollectChromeFile)))
    return base::TimeDelta::FromSeconds(kKillTimeoutCollectChrome);
  else
    return kill_timeout_;
}

bool SessionManagerService::InitializeImpl() {
  if (!impl_->Initialize()) {
    LOG(ERROR) << "Policy key is likely corrupt. Initiating device wipe.";
    impl_->InitiateDeviceWipe();
    impl_->Finalize();
    exit_code_ = MUST_WIPE_DEVICE;
    return false;
  }
  return true;
}

void SessionManagerService::AllowGracefulExitOrRunForever() {
  if (exit_on_child_done_) {
    LOG(INFO) << "SessionManagerService set to exit on child done";
    loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&SessionManagerService::ScheduleShutdown),
                   this));
  } else {
    DLOG(INFO) << "OK, running forever...";
  }
}

void SessionManagerService::SetExitAndScheduleShutdown(ExitCode code) {
  shutting_down_ = true;
  exit_code_ = code;
  impl_->AnnounceSessionStoppingIfNeeded();
  loop_proxy_->PostTask(FROM_HERE, quit_closure_);
  LOG(INFO) << "SessionManagerService quitting run loop";
}

void SessionManagerService::CleanupChildren(base::TimeDelta timeout) {
  RequestJobExit();
  key_gen_.RequestJobExit();
  EnsureJobExit(timeout);
  key_gen_.EnsureJobExit(timeout);
}

std::vector<std::string> SessionManagerService::GetArgList(
    const std::vector<std::string>& args) {
  std::vector<std::string>::const_iterator start_arg = args.begin();
  if (!args.empty() && *start_arg == "--")
    ++start_arg;
  return std::vector<std::string>(start_arg, args.end());
}

}  // namespace login_manager
