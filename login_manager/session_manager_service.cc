// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <grp.h>
#include <inttypes.h>
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
#include "login_manager/dbus_glib_shim.h"
#include "login_manager/device_local_account_policy_service.h"
#include "login_manager/device_management_backend.pb.h"
#include "login_manager/generator_job.h"
#include "login_manager/key_generator.h"
#include "login_manager/liveness_checker_impl.h"
#include "login_manager/login_metrics.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_store.h"
#include "login_manager/regen_mitigator.h"
#include "login_manager/session_manager_impl.h"
#include "login_manager/system_utils.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace login_manager {  // NOLINT
namespace gobject {  // NOLINT
#include "login_manager/server.h"
}  // namespace gobject
}  // namespace login_manager

namespace em = enterprise_management;
namespace login_manager {

int g_shutdown_pipe_write_fd = -1;
int g_shutdown_pipe_read_fd = -1;

int g_child_pipe_write_fd = -1;
int g_child_pipe_read_fd = -1;

namespace {
// I need a do-nothing action for SIGALRM, or using alarm() will kill me.
void DoNothing(int signal) {}

// Write |data| to |fd|, retrying on EINTR.
void RetryingWrite(int fd, const char* data, size_t data_length) {
  size_t written = 0;
  do {
    int rv = HANDLE_EINTR(write(fd, data + written, data_length - written));
    RAW_CHECK(rv >= 0);
    written += rv;
  } while (written < data_length);
}

// Common code between SIG{HUP,INT,TERM}Handler.
void GracefulShutdownHandler(int signal) {
  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  RAW_CHECK(g_shutdown_pipe_read_fd != -1);

  RetryingWrite(g_shutdown_pipe_write_fd,
                reinterpret_cast<const char*>(&signal),
                sizeof(signal));
  RAW_LOG(INFO,
          "Successfully wrote to shutdown pipe, resetting signal handler.");
}

void SIGHUPHandler(int signal) {
  RAW_CHECK(signal == SIGHUP);
  RAW_LOG(INFO, "Handling SIGHUP.");
  GracefulShutdownHandler(signal);
}

void SIGINTHandler(int signal) {
  RAW_CHECK(signal == SIGINT);
  RAW_LOG(INFO, "Handling SIGINT.");
  GracefulShutdownHandler(signal);
}

void SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  RAW_LOG(INFO, "Handling SIGTERM.");
  GracefulShutdownHandler(signal);
}

void SIGCHLDHandler(int signal, siginfo_t* info, void*) {
  RAW_CHECK(signal == SIGCHLD);
  RAW_LOG(INFO, "Handling SIGCHLD.");

  RAW_CHECK(g_child_pipe_write_fd != -1);
  RAW_CHECK(g_child_pipe_read_fd != -1);

  RetryingWrite(g_child_pipe_write_fd,
                reinterpret_cast<const char*>(info),
                sizeof(siginfo_t));
  RAW_LOG(INFO, "Successfully wrote to child pipe.");
}

}  // anonymous namespace

const char SessionManagerService::kFlagFileDir[] = "/var/run/session_manager";

// TODO(mkrebs): Remove CollectChrome timeout and file when
// crosbug.com/5872 is fixed.
// When crash-reporter based crash reporting of Chrome is enabled
// (which should only be during test runs) we use
// kKillTimeoutCollectChrome instead of the kill timeout specified at
// the command line.
const int SessionManagerService::kKillTimeoutCollectChrome = 60;
const char SessionManagerService::kCollectChromeFile[] =
    "/mnt/stateful_partition/etc/collect_chrome_crashes";

namespace {

// Device-local account state directory.
const FilePath::CharType kDeviceLocalAccountStateDir[] =
    FILE_PATH_LITERAL("/var/lib/device_local_accounts");

}  // namespace

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
      base::Bind(&SessionManagerService::HandleChildExit,
                 session_manager_service_, info));
}

SessionManagerService::SessionManagerService(
    scoped_ptr<BrowserJobInterface> child_job,
    const base::Closure& quit_closure,
    int kill_timeout,
    bool enable_browser_abort_on_hang,
    base::TimeDelta hang_detection_interval,
    SystemUtils* utils)
    : browser_(child_job.Pass()),
      exit_on_child_done_(false),
      kill_timeout_(base::TimeDelta::FromSeconds(kill_timeout)),
      session_manager_(NULL),
      main_loop_(NULL),
      loop_proxy_(NULL),
      quit_closure_(quit_closure),
      system_(utils),
      nss_(NssUtil::Create()),
      key_gen_(new KeyGenerator(utils, this)),
      login_metrics_(NULL),
      liveness_checker_(NULL),
      enable_browser_abort_on_hang_(enable_browser_abort_on_hang),
      liveness_checking_interval_(hang_detection_interval),
      set_uid_(false),
      shutting_down_(false),
      shutdown_already_(false),
      exit_code_(SUCCESS) {
  int pipefd[2];
  PLOG_IF(DFATAL, pipe2(pipefd, O_CLOEXEC) < 0) << "Failed to create pipe";
  g_shutdown_pipe_read_fd = pipefd[0];
  g_shutdown_pipe_write_fd = pipefd[1];

  PLOG_IF(FATAL, pipe2(pipefd,
                       O_CLOEXEC | O_NONBLOCK) < 0) << "Failed to create pipe";
  g_child_pipe_read_fd = pipefd[0];
  g_child_pipe_write_fd = pipefd[1];

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

  FilePath flag_file_dir(kFlagFileDir);
  if (!file_util::CreateDirectory(flag_file_dir)) {
    PLOG(ERROR) << "Cannot create flag file directory at " << kFlagFileDir;
    return false;
  }
  login_metrics_.reset(new LoginMetrics(flag_file_dir));
  reinterpret_cast<BrowserJob*>(browser_.get())->set_login_metrics(
      login_metrics_.get());

  // Initially store in derived-type pointer, so that we can initialize
  // appropriately below, and also use as delegate for device_policy.
  SessionManagerImpl* impl =
      new SessionManagerImpl(
          scoped_ptr<UpstartSignalEmitter>(new UpstartSignalEmitter),
          this, login_metrics_.get(), nss_.get(), system_);

  // The below require loop_proxy_, created in Reset(), to be set already.
  liveness_checker_.reset(
      new LivenessCheckerImpl(this,
                              system_,
                              loop_proxy_,
                              enable_browser_abort_on_hang_,
                              liveness_checking_interval_));


  owner_key_.reset(new PolicyKey(nss_->GetOwnerKeyFilePath(), nss_.get()));
  scoped_ptr<OwnerKeyLossMitigator> mitigator(
      new RegenMitigator(key_gen_.get(), set_uid_, uid_));
  scoped_ptr<DevicePolicyService> device_policy(
      DevicePolicyService::Create(login_metrics_.get(),
                                  owner_key_.get(),
                                  mitigator.Pass(),
                                  nss_.get(),
                                  loop_proxy_));
  device_policy->set_delegate(impl);

  scoped_ptr<UserPolicyServiceFactory> user_policy_factory(
      new UserPolicyServiceFactory(getuid(), loop_proxy_, nss_.get(), system_));
  scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy(
      new DeviceLocalAccountPolicyService(FilePath(kDeviceLocalAccountStateDir),
                                          owner_key_.get(),
                                          loop_proxy_));
  impl->InjectPolicyServices(device_policy.Pass(),
                             user_policy_factory.Pass(),
                             device_local_account_policy.Pass());
  impl_.reset(impl);

  if (!InitializeImpl())
    return false;

  // Set any flags that were specified system-wide.
  if (device_policy)
    browser_->SetExtraArguments(device_policy->GetStartUpFlags());

  g_io_add_watch_full(g_io_channel_unix_new(g_shutdown_pipe_read_fd),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP),
                      HandleKill,
                      this,
                      NULL);

  g_io_add_watch_full(g_io_channel_unix_new(g_child_pipe_read_fd),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP),
                      HandleChildrenExiting,
                      this,
                      NULL);

  // Wire impl to dbus-glib glue.
  if (session_manager_)
    g_object_unref(session_manager_);
  session_manager_ =
      reinterpret_cast<gobject::SessionManager*>(
          g_object_new(gobject::session_manager_get_type(), NULL));
  session_manager_->impl = impl_.get();

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
  loop_proxy_ = MessageLoop::current()->message_loop_proxy();
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
  liveness_checker_->Start();
}

void SessionManagerService::AbortBrowser(int signal,
                                         const std::string& message) {
  browser_->KillEverything(signal, message);
}

void SessionManagerService::RestartBrowserWithArgs(
    const std::vector<std::string>& args, bool args_are_extra) {
  // Waiting for Chrome to shutdown takes too much time.
  // We're killing it immediately hoping that data Chrome uses before
  // logging in is not corrupted.
  // TODO(avayvod): Remove RestartJob when crosbug.com/6924 is fixed.
  browser_->KillEverything(SIGKILL, "");
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

void SessionManagerService::RunKeyGenerator(const std::string& username) {
  if (!key_gen_->Start(username, set_uid_ ? uid_ : 0))
    LOG(ERROR) << "Can't fork to run key generator.";
}

void SessionManagerService::AdoptKeyGeneratorJob(
    scoped_ptr<GeneratorJobInterface> job,
    pid_t pid) {
  DLOG(INFO) << "Adopting key generator job with pid " << pid;
  generator_.swap(job);
}

void SessionManagerService::AbandonKeyGeneratorJob() {
  generator_.reset(NULL);
}

void SessionManagerService::ProcessNewOwnerKey(const std::string& username,
                                               const FilePath& key_file) {
  DLOG(INFO) << "Processing generated key at " << key_file.value();
  impl_->ImportValidateAndStoreGeneratedKey(username, key_file);
}

bool SessionManagerService::IsGenerator(pid_t pid) {
  return (generator_ &&
          generator_->CurrentPid() > 0 &&
          pid == generator_->CurrentPid());
}

bool SessionManagerService::IsBrowser(pid_t pid) {
  return (browser_ &&
          browser_->CurrentPid() > 0 &&
          pid == browser_->CurrentPid());
}

bool SessionManagerService::IsManagedProcess(pid_t pid) {
  return IsBrowser(pid) || IsGenerator(pid);
}

void SessionManagerService::HandleBrowserExit() {
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

void SessionManagerService::HandleChildExit(const siginfo_t& info) {
  CHECK(system_->ChildIsGone(info.si_pid, base::TimeDelta::FromSeconds(5)));
  if (!IsManagedProcess(info.si_pid))
    return;

  if (shutting_down_)
    return;

  std::string job_name;
  if (IsBrowser(info.si_pid))
    job_name = browser_->GetName();
  else if (IsGenerator(info.si_pid))
    job_name = generator_->GetName();

  LOG(INFO) << "Handling " << job_name << "(" << info.si_pid << ") exit.";
  if (info.si_code == CLD_EXITED) {
    LOG_IF(ERROR, info.si_status != 0) << "  Exited with exit code "
                                       << info.si_status;
    CHECK(info.si_status != ChildJobInterface::kCantSetUid);
    CHECK(info.si_status != ChildJobInterface::kCantExec);
  } else {
    LOG(ERROR) << "  Exited with signal " << info.si_status;
  }

  if (IsBrowser(info.si_pid))
    HandleBrowserExit();
  else if (IsGenerator(info.si_pid))
    key_gen_->HandleExit(info.si_status == 0);
}

///////////////////////////////////////////////////////////////////////////////
// glib event handlers

// static
gboolean SessionManagerService::HandleChildrenExiting(GIOChannel* source,
                                                      GIOCondition condition,
                                                      gpointer data) {
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  siginfo_t info;
  while (file_util::ReadFromFD(g_io_channel_unix_get_fd(source),
                               reinterpret_cast<char*>(&info),
                               sizeof(info))) {
    DCHECK(info.si_signo == SIGCHLD) << "Wrong signal!";
    manager->HandleChildExit(info);
  }
  return TRUE;  // So that the event source that called this remains installed.
}

gboolean SessionManagerService::HandleKill(GIOChannel* source,
                                           GIOCondition condition,
                                           gpointer data) {
  // We only get called if there's data on the pipe.  If there's data, we're
  // supposed to exit.  So, don't even bother to read it.
  LOG(INFO) << "HUP, INT, or TERM received; exiting.";

  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  manager->ScheduleShutdown();
  return FALSE;  // So that the event source that called this gets removed.
}

///////////////////////////////////////////////////////////////////////////////
// Utility Methods

// static
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

  // For all termination handlers, we want the default re-installed after
  // we get a shot at handling the signal.
  action.sa_flags = SA_RESETHAND;

  // We need to handle SIGTERM, because that is how many POSIX-based distros ask
  // processes to quit gracefully at shutdown time.
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  // Also handle SIGINT, if session_manager is being run in the foreground.
  action.sa_handler = SIGINTHandler;
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
  // distros send SIGHUP, SIGTERM, and then SIGKILL.
  action.sa_handler = SIGHUPHandler;
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);

  // Manually set an action for SIGCHLD, so that we can convert
  // receiving a signal on child exit to file descriptor
  // activity. This way we can handle this kind of event in a
  // MessageLoop.  The flags are set such that the action receives a
  // siginfo struct with handy exit status info, but only when the
  // child actually exits -- as opposed to also when it gets STOP or CONT.
  memset(&action, 0, sizeof(action));
  action.sa_flags = (SA_SIGINFO | SA_NOCLDSTOP);
  action.sa_sigaction = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);
}

void SessionManagerService::RevertHandlers() {
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);
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
    impl_->StartDeviceWipe(NULL, NULL);
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
    DLOG(INFO) << "Ok, running forever...";
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
  if (browser_ && browser_->CurrentPid() > 0)
    browser_->Kill(SIGTERM, "");
  if (generator_ && generator_->CurrentPid() > 0)
    generator_->Kill(SIGTERM, "");

  if (browser_ && browser_->CurrentPid() > 0)
    browser_->WaitAndAbort(timeout);
  if (generator_ && generator_->CurrentPid() > 0)
    generator_->WaitAndAbort(timeout);
}

// static
std::vector<std::string> SessionManagerService::GetArgList(
    const std::vector<std::string>& args) {
  std::vector<std::string>::const_iterator start_arg = args.begin();
  if (!args.empty() && *start_arg == "--")
    ++start_arg;
  return std::vector<std::string>(start_arg, args.end());
}

}  // namespace login_manager
