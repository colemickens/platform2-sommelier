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
#include <chromeos/utility.h>

#include "login_manager/child_job.h"
#include "login_manager/dbus_glib_shim.h"
#include "login_manager/device_local_account_policy_service.h"
#include "login_manager/device_management_backend.pb.h"
#include "login_manager/key_generator.h"
#include "login_manager/liveness_checker_impl.h"
#include "login_manager/login_metrics.h"
#include "login_manager/nss_util.h"
#include "login_manager/policy_store.h"
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

using std::make_pair;
using std::pair;
using std::string;
using std::vector;

int g_shutdown_pipe_write_fd = -1;
int g_shutdown_pipe_read_fd = -1;

// static
// Common code between SIG{HUP, INT, TERM}Handler.
void SessionManagerService::GracefulShutdownHandler(int signal) {
  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  RAW_CHECK(sigaction(signal, &action, NULL) == 0);

  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  RAW_CHECK(g_shutdown_pipe_read_fd != -1);

  size_t bytes_written = 0;
  do {
    int rv = HANDLE_EINTR(
        write(g_shutdown_pipe_write_fd,
              reinterpret_cast<const char*>(&signal) + bytes_written,
              sizeof(signal) - bytes_written));
    RAW_CHECK(rv >= 0);
    bytes_written += rv;
  } while (bytes_written < sizeof(signal));

  RAW_LOG(INFO,
          "Successfully wrote to shutdown pipe, resetting signal handler.");
}

// static
void SessionManagerService::SIGHUPHandler(int signal) {
  RAW_CHECK(signal == SIGHUP);
  RAW_LOG(INFO, "Handling SIGHUP.");
  GracefulShutdownHandler(signal);
}
// static
void SessionManagerService::SIGINTHandler(int signal) {
  RAW_CHECK(signal == SIGINT);
  RAW_LOG(INFO, "Handling SIGINT.");
  GracefulShutdownHandler(signal);
}

// static
void SessionManagerService::SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  RAW_LOG(INFO, "Handling SIGTERM.");
  GracefulShutdownHandler(signal);
}

const char SessionManagerService::kFirstBootFlag[] = "--first-boot";

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
  session_manager_service_->loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&HandleBrowserExit,
                 pid,
                 status,
                 reinterpret_cast<void*>(session_manager_service_)));
}

SessionManagerService::SessionManagerService(
    scoped_ptr<ChildJobInterface> child_job,
    int kill_timeout,
    bool enable_browser_abort_on_hang,
    base::TimeDelta hang_detection_interval,
    SystemUtils* utils)
    : browser_(child_job.Pass()),
      exit_on_child_done_(false),
      kill_timeout_(kill_timeout),
      session_manager_(NULL),
      main_loop_(NULL),
      dont_use_directly_(NULL),
      loop_proxy_(NULL),
      system_(utils),
      nss_(NssUtil::Create()),
      key_gen_(new KeyGenerator(utils)),
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

  SetupHandlers();
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

  // Initially store in derived-type pointer, so that we can initialize
  // appropriately below, and also use as delegate for device_policy_.
  SessionManagerImpl* impl =
      new SessionManagerImpl(
          scoped_ptr<UpstartSignalEmitter>(new UpstartSignalEmitter),
          this, login_metrics_.get(), system_);

  // The below require loop_proxy_, created in Reset(), to be set already.
  liveness_checker_.reset(
      new LivenessCheckerImpl(this,
                              system_,
                              loop_proxy_,
                              enable_browser_abort_on_hang_,
                              liveness_checking_interval_));

  owner_key_.reset(new PolicyKey(nss_->GetOwnerKeyFilePath()));
  scoped_refptr<DevicePolicyService> device_policy(
      DevicePolicyService::Create(login_metrics_.get(),
                                  owner_key_.get(),
                                  mitigator_.Pass(),
                                  nss_.get(),
                                  loop_proxy_));
  device_policy->set_delegate(impl);

  scoped_ptr<UserPolicyServiceFactory> user_policy_factory(
      new UserPolicyServiceFactory(getuid(), loop_proxy_, system_));
  scoped_ptr<DeviceLocalAccountPolicyService> device_local_account_policy(
      new DeviceLocalAccountPolicyService(FilePath(kDeviceLocalAccountStateDir),
                                          owner_key_.get(),
                                          loop_proxy_));
  impl->InjectPolicyServices(device_policy,
                             user_policy_factory.Pass(),
                             device_local_account_policy.Pass());
  impl_.reset(impl);

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
  const string filter =
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
  dont_use_directly_.reset(NULL);
  dont_use_directly_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  loop_proxy_ = dont_use_directly_->message_loop_proxy();
  return true;
}

int SessionManagerService::GetKillTimeout() {
  if (file_util::PathExists(FilePath(kCollectChromeFile)))
    return kKillTimeoutCollectChrome;
  else
    return kill_timeout_;
}

bool SessionManagerService::Run() {
  if (!main_loop_) {
    LOG(ERROR) << "You must have a main loop to call Run.";
    return false;
  }
  g_io_add_watch_full(g_io_channel_unix_new(g_shutdown_pipe_read_fd),
                      G_PRIORITY_HIGH_IDLE,
                      GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP),
                      HandleKill,
                      this,
                      NULL);
  if (ShouldRunBrowser())  // Allows devs to start/stop browser manually.
    RunBrowser();

  // Initializes policy subsystems which, among other things, finds and
  // validates the stored policy signing key if one is present.
  // A corrupted policy key means that the device needs to undergo
  // 'Powerwash', which reboots and then wipes most of the stateful partition.
  if (!impl_->Initialize()) {
    impl_->StartDeviceWipe(NULL, NULL);
    Finalize();
    return false;
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();  // Will return when quit_closure_ is posted and run.
  CleanupChildren(GetKillTimeout());
  impl_->AnnounceSessionStopped();
  return true;
}

bool SessionManagerService::ShouldRunBrowser() {
  return !file_checker_.get() || !file_checker_->exists();
}

bool SessionManagerService::ShouldStopChild(ChildJobInterface* child_job) {
  return child_job->ShouldStop();
}

bool SessionManagerService::Shutdown() {
  Finalize();
  loop_proxy_->PostTask(FROM_HERE, quit_closure_);
  LOG(INFO) << "SessionManagerService quitting run loop";
  return true;
}

void SessionManagerService::ScheduleShutdown() {
  SetExitAndShutdown(SUCCESS);
}

void SessionManagerService::RunBrowser() {
  bool first_boot = !login_metrics_->HasRecordedChromeExec();

  login_metrics_->RecordStats("chrome-exec");
  if (first_boot)
    browser_.job->AddOneTimeArgument(kFirstBootFlag);
  LOG(INFO) << "Running child " << browser_.job->GetName() << "...";
  browser_.pid = RunChild(browser_.job.get());
  liveness_checker_->Start();
}

int SessionManagerService::RunChild(ChildJobInterface* child_job) {
  child_job->RecordTime();
  pid_t pid = system_->fork();
  if (pid == 0) {
    RevertHandlers();
    child_job->Run();
    exit(ChildJobInterface::kCantExec);  // Run() is not supposed to return.
  }
  child_job->ClearOneTimeArgument();

  browser_.watcher = g_child_watch_add_full(G_PRIORITY_HIGH_IDLE,
                                            pid,
                                            HandleBrowserExit,
                                            this,
                                            NULL);
  return pid;
}

void SessionManagerService::AbortBrowser() {
  KillChild(browser_.job.get(), browser_.pid, SIGABRT);
}

void SessionManagerService::RestartBrowserWithArgs(
    const vector<string>& args, bool args_are_extra) {
  // Waiting for Chrome to shutdown takes too much time.
  // We're killing it immediately hoping that data Chrome uses before
  // logging in is not corrupted.
  // TODO(avayvod): Remove RestartJob when crosbug.com/6924 is fixed.
  KillChild(browser_.job.get(), browser_.pid, SIGKILL);
  if (args_are_extra)
    browser_.job->SetExtraArguments(args);
  else
    browser_.job->SetArguments(args);
  RunBrowser();
}

void SessionManagerService::SetBrowserSessionForUser(const std::string& user) {
  browser_.job->StartSession(user);
}

void SessionManagerService::RunKeyGenerator() {
  key_gen_->Start(set_uid_ ? uid_ : 0, this);
}

void SessionManagerService::KillChild(const ChildJobInterface* child_job,
                                      int child_pid,
                                      int signal) {
  uid_t to_kill_as = getuid();
  if (child_job->IsDesiredUidSet())
    to_kill_as = child_job->GetDesiredUid();
  system_->kill(-child_pid, to_kill_as, signal);
  // Process will be reaped on the way into HandleBrowserExit.
}

void SessionManagerService::AdoptKeyGeneratorJob(
    scoped_ptr<ChildJobInterface> job,
    pid_t pid,
    guint watcher) {
  generator_.job.swap(job);
  generator_.pid = pid;
  generator_.watcher = watcher;
}

void SessionManagerService::AbandonKeyGeneratorJob() {
  if (generator_.pid > 0) {
    g_source_remove(generator_.watcher);
    generator_.watcher = 0;
  }
  generator_.job.reset(NULL);
  generator_.pid = -1;
}

bool SessionManagerService::IsBrowser(pid_t pid) {
  return pid == browser_.pid;
}

void SessionManagerService::AllowGracefulExit() {
  if (exit_on_child_done_) {
    shutting_down_ = true;
    LOG(INFO) << "SessionManagerService set to exit on child done";
    loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&SessionManagerService::Shutdown), this));
  }
}

///////////////////////////////////////////////////////////////////////////////
// glib event handlers

void SessionManagerService::HandleBrowserExit(GPid pid,
                                              gint status,
                                              gpointer data) {
  // If I could wait for descendants here, I would.  Instead, I kill them.
  kill(-pid, SIGKILL);

  DLOG(INFO) << "Handling child process exit: " << pid;
  if (WIFSIGNALED(status)) {
    DLOG(INFO) << "  Exited with signal " << WTERMSIG(status);
  } else if (WIFEXITED(status)) {
    DLOG(INFO) << "  Exited with exit code " << WEXITSTATUS(status);
    CHECK(WEXITSTATUS(status) != ChildJobInterface::kCantSetUid);
    CHECK(WEXITSTATUS(status) != ChildJobInterface::kCantExec);
  } else {
    DLOG(INFO) << "  Exited...somehow, without an exit code or a signal??";
  }

  // If the child _ever_ exits uncleanly, we want to start it up again.
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);

  // Do nothing if already shutting down.
  if (manager->shutting_down_)
    return;

  ChildJobInterface* child_job = NULL;
  if (manager->IsBrowser(pid))
    child_job = manager->browser_.job.get();

  LOG(ERROR) << StringPrintf("Process %s(%d) exited.",
                             child_job ? child_job->GetName().c_str() : "",
                             pid);
  if (manager->impl_->ScreenIsLocked()) {
    LOG(ERROR) << "Screen locked, shutting down";
    manager->SetExitAndShutdown(CRASH_WHILE_SCREEN_LOCKED);
    return;
  }

  if (child_job) {
    manager->liveness_checker_->Stop();
    if (manager->ShouldStopChild(child_job)) {
      LOG(WARNING) << "Child stopped, shutting down";
      manager->SetExitAndShutdown(CHILD_EXITING_TOO_FAST);
    } else if (manager->ShouldRunBrowser()) {
      // TODO(cmasone): deal with fork failing in RunBrowser()
      manager->RunBrowser();
    } else {
      LOG(INFO) << StringPrintf(
          "Should NOT run %s again...", child_job->GetName().data());
      manager->AllowGracefulExit();
    }
  } else {
    LOG(ERROR) << "Couldn't find pid of exiting child: " << pid;
  }
}

void SessionManagerService::HandleKeygenExit(GPid pid,
                                             gint status,
                                             gpointer data) {
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  manager->AbandonKeyGeneratorJob();

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    FilePath key_file(manager->key_gen_->temporary_key_filename());
    manager->impl_->ImportValidateAndStoreGeneratedKey(key_file);
  } else {
    if (WIFSIGNALED(status))
      LOG(ERROR) << "keygen exited on signal " << WTERMSIG(status);
    else
      LOG(ERROR) << "keygen exited with exit code " << WEXITSTATUS(status);
  }
}

gboolean SessionManagerService::HandleKill(GIOChannel* source,
                                           GIOCondition condition,
                                           gpointer data) {
  // We only get called if there's data on the pipe.  If there's data, we're
  // supposed to exit.  So, don't even bother to read it.
  LOG(INFO) << "SessionManagerService - data on pipe, so exiting";
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  manager->Shutdown();
  return FALSE;  // So that the event source that called this gets removed.
}

void SessionManagerService::Finalize() {
  LOG(INFO) << "SessionManagerService exiting";
  DeregisterChildWatchers();
  liveness_checker_->Stop();
  impl_->AnnounceSessionStoppingIfNeeded();
  impl_->Finalize();
}

void SessionManagerService::SetExitAndShutdown(ExitCode code) {
  exit_code_ = code;
  Shutdown();
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

void SessionManagerService::SetupHandlers() {
  // I have to ignore SIGUSR1, because Xorg sends it to this process when it's
  // got no clients and is ready for new ones.  If we don't ignore it, we die.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);

  action.sa_handler = SessionManagerService::do_nothing;
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);

  // We need to handle SIGTERM, because that is how many POSIX-based distros ask
  // processes to quit gracefully at shutdown time.
  action.sa_handler = SIGTERMHandler;
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  // Also handle SIGINT - when the user terminates the browser via Ctrl+C.
  // If the browser process is being debugged, GDB will catch the SIGINT first.
  action.sa_handler = SIGINTHandler;
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
  // distros send SIGHUP, SIGTERM, and then SIGKILL.
  action.sa_handler = SIGHUPHandler;
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);
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
}

void SessionManagerService::KillAndRemember(
    const ChildJob::Spec& spec,
    vector<std::pair<pid_t, uid_t> >* to_remember) {
  const pid_t pid = spec.pid;
  if (pid < 0)
    return;

  const uid_t uid = (spec.job->IsDesiredUidSet() ?
                     spec.job->GetDesiredUid() : getuid());
  system_->kill(pid, uid, SIGTERM);
  to_remember->push_back(make_pair(pid, uid));
}

void SessionManagerService::CleanupChildren(int timeout) {
  vector<pair<int, uid_t> > pids_to_abort;
  KillAndRemember(browser_, &pids_to_abort);
  KillAndRemember(generator_, &pids_to_abort);

  for (vector<pair<int, uid_t> >::const_iterator it = pids_to_abort.begin();
       it != pids_to_abort.end(); ++it) {
    const pid_t pid = it->first;
    const uid_t uid = it->second;
    if (!system_->ChildIsGone(pid, timeout)) {
      LOG(WARNING) << "Killing child process " << pid << " " << timeout
                   << " seconds after sending TERM signal";
      system_->kill(pid, uid, SIGABRT);
    } else {
      DLOG(INFO) << "Cleaned up child " << pid;
    }
  }
}

void SessionManagerService::DeregisterChildWatchers() {
  // Remove child exit handlers.
  if (browser_.pid > 0) {
    g_source_remove(browser_.watcher);
    browser_.watcher = 0;
  }
  if (generator_.pid > 0) {
    g_source_remove(generator_.watcher);
    generator_.watcher = 0;
  }
}

// static
vector<string> SessionManagerService::GetArgList(const vector<string>& args) {
  vector<string>::const_iterator start_arg = args.begin();
  if (!args.empty() && *start_arg == "--")
    ++start_arg;
  return vector<string>(start_arg, args.end());
}

}  // namespace login_manager
