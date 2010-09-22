// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/session_manager_service.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <errno.h>
#include <glib.h>
#include <grp.h>
#include <secder.h>
#include <signal.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/scoped_ptr.h"
#include "chromeos/dbus/dbus.h"
#include "chromeos/dbus/service_constants.h"

#include "login_manager/child_job.h"
#include "login_manager/interface.h"
#include "login_manager/nss_util.h"
#include "login_manager/owner_key.h"

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace login_manager {  // NOLINT
namespace gobject {  // NOLINT
#include "login_manager/bindings/server.h"
}  // namespace gobject
}  // namespace login_manager

namespace login_manager {

using std::string;
using std::vector;

// Jacked from chrome base/eintr_wrapper.h
#define HANDLE_EINTR(x) ({ \
  typeof(x) __eintr_result__; \
  do { \
    __eintr_result__ = x; \
  } while (__eintr_result__ == -1 && errno == EINTR); \
  __eintr_result__;\
})

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

//static
const uint32 SessionManagerService::kMaxEmailSize = 200;
//static
const char SessionManagerService::kEmailSeparator = '@';
//static
const char SessionManagerService::kLegalCharacters[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    ".@1234567890";
//static
const char SessionManagerService::kIncognitoUser[] = "";

namespace {

// Time we wait for child job to die (in seconds).
const int kKillTimeout = 3;
const int kMaxArgumentsSize = 512;

}  // namespace

SessionManagerService::SessionManagerService(
    std::vector<ChildJobInterface*> child_jobs)
    : child_jobs_(child_jobs.begin(), child_jobs.end()),
      child_pids_(child_jobs.size(), -1),
      exit_on_child_done_(false),
      session_manager_(NULL),
      main_loop_(g_main_loop_new(NULL, FALSE)),
      system_(new SystemUtils),
      nss_(NssUtil::Create()),
      key_(new OwnerKey(nss_->GetOwnerKeyFilePath())),
      store_(new PrefStore(FilePath(PrefStore::kDefaultPath))),
      session_started_(false),
      screen_locked_(false),
      set_uid_(false),
      shutting_down_(false) {
  SetupHandlers();
}

SessionManagerService::~SessionManagerService() {
  if (main_loop_)
    g_main_loop_unref(main_loop_);
  if (session_manager_)
    g_object_unref(session_manager_);

  // Remove this in case it was added by StopSession().
  g_idle_remove_by_data(this);

  // Remove this in case it was added by SetOwnerKey().
  g_idle_remove_by_data(key_.get());

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  CHECK(sigaction(SIGUSR1, &action, NULL) == 0);
  CHECK(sigaction(SIGALRM, &action, NULL) == 0);
  CHECK(sigaction(SIGTERM, &action, NULL) == 0);
  CHECK(sigaction(SIGINT, &action, NULL) == 0);
  CHECK(sigaction(SIGHUP, &action, NULL) == 0);

  STLDeleteElements(&child_jobs_);
}

bool SessionManagerService::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(
      gobject::session_manager_get_type(),
      &gobject::dbus_glib_session_manager_object_info);

  // Creates D-Bus GLib signal ids.
  signals_[kSignalSessionStateChanged] =
      g_signal_new("session_state_changed",
                   gobject::session_manager_get_type(),
                   G_SIGNAL_RUN_LAST,
                   0,
                   NULL, NULL,
                   g_cclosure_marshal_VOID__STRING,
                   G_TYPE_NONE, 1, G_TYPE_STRING);

  if (!store_->LoadOrCreate())
    LOG(ERROR) << "Could not load existing settings.  Continuing anyway...";
  return Reset();
}

bool SessionManagerService::Reset() {
  if (session_manager_)
    g_object_unref(session_manager_);
  session_manager_ =
      reinterpret_cast<gobject::SessionManager*>(
          g_object_new(gobject::session_manager_get_type(), NULL));

  // Allow references to this instance.
  session_manager_->service = this;

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
  if (!main_loop_) {
    LOG(ERROR) << "You must have a main loop to call Run.";
    return false;
  }

  int pipefd[2];
  int ret = pipe(pipefd);
  if (ret < 0) {
    PLOG(DFATAL) << "Failed to create pipe";
  } else {
    g_shutdown_pipe_read_fd = pipefd[0];
    g_shutdown_pipe_write_fd = pipefd[1];
    g_io_add_watch_full(g_io_channel_unix_new(g_shutdown_pipe_read_fd),
                        G_PRIORITY_HIGH_IDLE,
                        GIOCondition(G_IO_IN | G_IO_PRI | G_IO_HUP),
                        HandleKill,
                        this,
                        NULL);
  }

  if (ShouldRunChildren()) {
    RunChildren();
  } else {
    AllowGracefulExit();
  }

  // TODO(cmasone): A corrupted owner key means that the user needs to go
  //                to recovery mode.  How to tell them that from here?
  DCHECK(key_->PopulateFromDiskIfPossible());

  g_main_loop_run(main_loop_);

  CleanupChildren(kKillTimeout);

  return true;
}

bool SessionManagerService::ShouldRunChildren() {
  return !file_checker_.get() || !file_checker_->exists();
}

bool SessionManagerService::ShouldStopChild(ChildJobInterface* child_job) {
  return child_job->ShouldStop();
}

bool SessionManagerService::Shutdown() {
  if (session_started_) {
    DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:stopped";
    g_signal_emit(session_manager_,
                  signals_[kSignalSessionStateChanged],
                  0, "stopped");
  }

  // Even if we haven't gotten around to processing a persist task.
  store_->Persist();

  return chromeos::dbus::AbstractDbusService::Shutdown();
}

// Output uptime and disk stats to a file
static void RecordStats(ChildJobInterface* job) {
  // File uptime logs are located in.
  static const char kLogPath[] = "/tmp";
  // Prefix for the time measurement files.
  static const char kUptimePrefix[] = "uptime-";
  // Prefix for the disk usage files.
  static const char kDiskPrefix[] = "disk-";
  // The location of the current uptime stats.
  static const FilePath kProcUptime("/proc/uptime");
  // The location the the current disk stats.
  static const FilePath kDiskStat("/sys/block/sda/stat");
  // Suffix for both uptime and disk stats
  static const char kSuffix[] = "-exec";
  std::string job_name = job->GetName();
  if (job_name.size()) {
    FilePath log_dir(kLogPath);
    FilePath uptime_file = log_dir.Append(kUptimePrefix + job_name + kSuffix);
    if (!file_util::PathExists(uptime_file)) {
      std::string uptime;

      file_util::ReadFileToString(kProcUptime, &uptime);
      file_util::WriteFile(uptime_file, uptime.data(), uptime.size());
    }
    FilePath disk_file = log_dir.Append(kDiskPrefix + job_name + kSuffix);
    if (!file_util::PathExists(disk_file)) {
      std::string disk;

      file_util::ReadFileToString(kDiskStat, &disk);
      file_util::WriteFile(disk_file, disk.data(), disk.size());
    }
  }
}

void SessionManagerService::RunChildren() {
  for (size_t i_child = 0; i_child < child_jobs_.size(); ++i_child) {
    ChildJobInterface* child_job = child_jobs_[i_child];
    LOG(INFO) << "Running child " << child_job->GetName() << "...";
    RecordStats(child_job);
    child_pids_[i_child] = RunChild(child_job);
  }
}

int SessionManagerService::RunChild(ChildJobInterface* child_job) {
  child_job->RecordTime();
  int pid = fork();
  if (pid == 0) {
    child_job->Run();
    exit(1);  // Run() is not supposed to return.
  }
  g_child_watch_add_full(G_PRIORITY_HIGH_IDLE,
                         pid,
                         HandleChildExit,
                         this,
                         NULL);
  return pid;
}

void SessionManagerService::AllowGracefulExit() {
  shutting_down_ = true;
  if (exit_on_child_done_) {
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    ServiceShutdown,
                    this,
                    NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// SessionManagerService commands

gboolean SessionManagerService::EmitLoginPromptReady(gboolean* OUT_emitted,
                                                     GError** error) {
  DLOG(INFO) << "emitting login-prompt-ready ";
  *OUT_emitted = system("/sbin/initctl emit login-prompt-ready &") == 0;
  if (*OUT_emitted) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_EMIT_FAILED,
              "Can't emit login-prompt-ready.");
  }
  return *OUT_emitted;
}

gboolean SessionManagerService::StartSession(gchar* email_address,
                                             gchar* unique_identifier,
                                             gboolean* OUT_done,
                                             GError** error) {
  if (session_started_) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_SESSION_EXISTS,
              "Can't start a session while a session is already active.");
    *OUT_done = FALSE;
    return FALSE;
  }
  // basic validity checking; avoid buffer overflows here, and
  // canonicalize the email address a little.
  char email[kMaxEmailSize + 1];
  snprintf(email, sizeof(email), "%s", email_address);
  email[kMaxEmailSize] = '\0';  // Just to be sure.
  string email_string(email);
  if (email_string != kIncognitoUser && !ValidateEmail(email_string)) {
    *OUT_done = FALSE;
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_INVALID_EMAIL,
              "Provided email address is not valid.  ASCII only.");
    return FALSE;
  }
  string email_lower = StringToLowerASCII(email_string);
  DLOG(INFO) << "emitting start-user-session for " << email_lower;
  string command;
  if (set_uid_) {
    command = StringPrintf("/sbin/initctl emit start-user-session "
                           "CHROMEOS_USER=%s USER_ID=%d &",
                           email_lower.c_str(), uid_);
  } else {
    command = StringPrintf("/sbin/initctl emit start-user-session "
                           "CHROMEOS_USER=%s &",
                           email_lower.c_str());
  }
  // TODO(yusukes,cmasone): set DATA_DIR variable as well?

  *OUT_done = system(command.c_str()) == 0;
  if (*OUT_done) {
    for (size_t i_child = 0; i_child < child_jobs_.size(); ++i_child) {
      ChildJobInterface* child_job = child_jobs_[i_child];
      child_job->StartSession(email_lower);
    }
    session_started_ = true;

    DLOG(INFO) << "emitting D-Bus signal SessionStateChanged:started";
    g_signal_emit(session_manager_,
                  signals_[kSignalSessionStateChanged],
                  0, "started");
  } else {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_EMIT_FAILED,
              "Can't emit start-session.");
  }
  return *OUT_done;
}

gboolean SessionManagerService::StopSession(gchar* unique_identifier,
                                            gboolean* OUT_done,
                                            GError** error) {
  g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                  ServiceShutdown,
                  this,
                  NULL);
  // TODO(cmasone): re-enable these when we try to enable logout without exiting
  //                the session manager
  // child_job_->StopSession();
  // session_started_ = false;
  return *OUT_done = TRUE;
}

gboolean SessionManagerService::SetOwnerKey(GArray* public_key_der,
                                            GError** error) {
  LOG(INFO) << "key size is " << public_key_der->len;

  if (!nss_->OpenUserDB()) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_NO_USER_NSSDB,
              "Could not open the current user's NSS database.");
    return FALSE;
  }

  std::vector<uint8> pub_key;
  NssUtil::KeyFromBuffer(public_key_der, &pub_key);

  if (!nss_->CheckOwnerKey(pub_key)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY,
              "Could not verify that public key belongs to the owner.");
    return FALSE;
  }

  if (!key_->PopulateFromBuffer(pub_key)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY,
              "Illegal attempt to set the owner's public key.");
    return FALSE;
  }
  g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                  PersistKey,
                  key_.get(),
                  NULL);
  return TRUE;
}

gboolean SessionManagerService::Unwhitelist(gchar* email_address,
                                            GArray* signature,
                                            GError** error) {
  LOG(INFO) << "Unwhitelisting " << email_address;
  if (!key_->IsPopulated()) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_NO_OWNER_KEY,
              "Attempt to unwhitelist before owner's key is set.");
    return FALSE;
  }
  if (!key_->Verify(email_address,
                    strlen(email_address),
                    signature->data,
                    signature->len)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_VERIFY_FAIL,
              "Signature could not be verified.");
    return FALSE;
  }
  store_->Unwhitelist(email_address);
  g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                  PersistWhitelist,
                  store_.get(),
                  NULL);
  return TRUE;
}

gboolean SessionManagerService::CheckWhitelist(gchar* email_address,
                                               GArray** OUT_signature,
                                               GError** error) {
  std::string encoded;
  if (!store_->GetFromWhitelist(email_address, &encoded)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_ILLEGAL_USER,
              "The user is not whitelisted.");
    return FALSE;
  }
  std::string decoded;
  if (!base::Base64Decode(encoded, &decoded)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_DECODE_FAIL,
              "Signature could not be decoded.");
    return FALSE;
  }

  *OUT_signature = g_array_sized_new(FALSE, FALSE, 1, decoded.length());
  g_array_append_vals(*OUT_signature, decoded.c_str(), decoded.length());
  return TRUE;
}

gboolean SessionManagerService::EnumerateWhitelisted(gchar*** OUT_whitelist,
                                                     GError** error) {
  std::vector<std::string> the_whitelisted;
  store_->EnumerateWhitelisted(&the_whitelisted);
  uint num_whitelisted = the_whitelisted.size();
  *OUT_whitelist = g_new(gchar*, num_whitelisted + 1);
  (*OUT_whitelist)[num_whitelisted] = NULL;

  for (uint i = 0; i < num_whitelisted; ++i) {
    (*OUT_whitelist)[i] = g_strndup(the_whitelisted[i].c_str(),
                                    the_whitelisted[i].length());
  }
  return TRUE;
}

gboolean SessionManagerService::Whitelist(gchar* email_address,
                                          GArray* signature,
                                          GError** error) {
  LOG(INFO) << "Whitelisting " << email_address;
  if (!key_->IsPopulated()) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_NO_OWNER_KEY,
              "Attempt to whitelist before owner's key is set.");
    return FALSE;
  }
  if (!key_->Verify(email_address,
                    strlen(email_address),
                    signature->data,
                    signature->len)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_VERIFY_FAIL,
              "Signature could not be verified.");
    return FALSE;
  }
  std::string data(signature->data, signature->len);
  std::string encoded;
  if (!base::Base64Encode(data, &encoded)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_ENCODE_FAIL,
              "Signature could not be encoded.");
    return FALSE;
  }
  store_->Whitelist(email_address, encoded);
  g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                  PersistWhitelist,
                  store_.get(),
                  NULL);
  return TRUE;
}

gboolean SessionManagerService::StoreProperty(gchar* name,
                                              gchar* value,
                                              GArray* signature,
                                              GError** error) {
  LOG(INFO) << "Setting pref " << name << "=" << value;
  if (!key_->IsPopulated()) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_NO_OWNER_KEY,
              "Attempt to whitelist before owner's key is set.");
    return FALSE;
  }
  std::string was_signed = base::StringPrintf("%s=%s", name, value);
  if (!key_->Verify(was_signed.c_str(),
                    was_signed.length(),
                    signature->data,
                    signature->len)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_VERIFY_FAIL,
              "Signature could not be verified.");
    return FALSE;
  }
  std::string data(signature->data, signature->len);
  std::string encoded;
  if (!base::Base64Encode(data, &encoded)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_ENCODE_FAIL,
              "Signature could not be verified.");
    return FALSE;
  }
  store_->Set(name, value, encoded);
  g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                  PersistStore,
                  store_.get(),
                  NULL);
  return TRUE;
}

gboolean SessionManagerService::RetrieveProperty(gchar* name,
                                                 gchar** OUT_value,
                                                 GArray** OUT_signature,
                                                 GError** error) {
  std::string encoded;
  std::string value;
  if (!store_->Get(name, &value, &encoded)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_UNKNOWN_PROPERTY,
              "The requested property is unknown.");
    return FALSE;
  }
  *OUT_value = g_strdup_printf("%.*s", value.length(), value.c_str());

  std::string decoded;
  if (!base::Base64Decode(encoded, &decoded)) {
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_DECODE_FAIL,
              "Signature could not be decoded.");
    return FALSE;
  }
  *OUT_signature = g_array_sized_new(FALSE, FALSE, 1, decoded.length());
  g_array_append_vals(*OUT_signature, decoded.c_str(), decoded.length());

  return TRUE;
}

gboolean SessionManagerService::LockScreen(GError** error) {
  screen_locked_ = TRUE;
  SendSignalToChromium(chromium::kLockScreenSignal, NULL);
  LOG(INFO) << "LockScreen";
  return TRUE;
}

gboolean SessionManagerService::UnlockScreen(GError** error) {
  screen_locked_ = FALSE;
  SendSignalToChromium(chromium::kUnlockScreenSignal, NULL);
  LOG(INFO) << "UnlockScreen";
  return TRUE;
}

gboolean SessionManagerService::RestartJob(gint pid,
                                           gchar* arguments,
                                           gboolean* OUT_done,
                                           GError** error) {
  int child_pid = pid;
  std::vector<int>::iterator child_pid_it =
      std::find(child_pids_.begin(), child_pids_.end(), child_pid);
  if (child_pid_it == child_pids_.end()) {
    *OUT_done = FALSE;
    SetGError(error,
              CHROMEOS_LOGIN_ERROR_UNKNOWN_PID,
              "Provided pid is unknown.");
    return FALSE;
  }

  // Waiting for Chrome to shutdown takes too much time.
  // We're killing it immediately hoping that data Chrome uses before
  // logging in is not corrupted.
  // TODO(avayvod): Remove RestartJob when crosbug.com/6924 is fixed.
  system_->kill(-child_pid, SIGKILL);

  char arguments_buffer[kMaxArgumentsSize + 1];
  snprintf(arguments_buffer, sizeof(arguments_buffer), "%s", arguments);
  arguments_buffer[kMaxArgumentsSize] = '\0';
  string arguments_string(arguments_buffer);

  size_t child_index = child_pid_it - child_pids_.begin();
  child_jobs_[child_index]->SetArguments(arguments_string);
  child_pids_[child_index] = RunChild(child_jobs_[child_index]);

  // To set "logged-in" state for BWSI mode.
  return StartSession(const_cast<gchar*>(kIncognitoUser), NULL,
                      OUT_done, error);
}

///////////////////////////////////////////////////////////////////////////////
// glib event handlers

void SessionManagerService::HandleChildExit(GPid pid,
                                            gint status,
                                            gpointer data) {
  // If I could wait for descendants here, I would.  Instead, I kill them.
  kill(-pid, SIGKILL);

  DLOG(INFO) << "Handling child process exit.";
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

  int i_child = -1;
  for (int i = 0; i < manager->child_pids_.size(); ++i) {
    if (manager->child_pids_[i] == pid) {
      i_child = i;
      manager->child_pids_[i] = -1;
      break;
    }
  }

  ChildJobInterface* child_job = i_child >= 0
                                 ? manager->child_jobs_[i_child]
                                 : NULL;

  LOG(ERROR) << StringPrintf(
      "Process %s(%d) exited.",
      child_job ? child_job->GetName().c_str() : "",
      pid);
  if (manager->screen_locked_) {
    LOG(ERROR) << "Screen locked, shutting down",
    ServiceShutdown(data);
    return;
  }

  if (child_job) {
    if (manager->ShouldStopChild(child_job)) {
      ServiceShutdown(data);
    } else if (manager->ShouldRunChildren()) {
      // TODO(cmasone): deal with fork failing in RunChild()
      LOG(INFO) << StringPrintf(
          "Running child %s again...", child_job->GetName().data());
      manager->child_pids_[i_child] = manager->RunChild(child_job);
    } else {
      LOG(INFO) << StringPrintf(
          "Should NOT run %s again...", child_job->GetName().data());
      manager->AllowGracefulExit();
    }
  } else {
    LOG(ERROR) << "Couldn't find pid of exiting child: " << pid;
  }
}

gboolean SessionManagerService::HandleKill(GIOChannel* source,
                                           GIOCondition condition,
                                           gpointer data) {
  // We only get called if there's data on the pipe.  If there's data, we're
  // supposed to exit.  So, don't even bother to read it.
  return ServiceShutdown(data);
}

gboolean SessionManagerService::ServiceShutdown(gpointer data) {
  SessionManagerService* manager = static_cast<SessionManagerService*>(data);
  manager->Shutdown();
  LOG(INFO) << "SessionManagerService exiting";
  return FALSE;  // So that the event source that called this gets removed.
}

gboolean SessionManagerService::PersistKey(gpointer data) {
  OwnerKey* key = static_cast<OwnerKey*>(data);
  LOG(INFO) << "Persisting Owner key to disk.";
  if (key->Persist())
    SendSignalToChromium(chromium::kOwnerKeySetSignal, "success");
  else
    SendSignalToChromium(chromium::kOwnerKeySetSignal, "failure");
  return FALSE;  // So that the event source that called this gets removed.
}

gboolean SessionManagerService::PersistWhitelist(gpointer data) {
  PrefStore* store = static_cast<PrefStore*>(data);
  LOG(INFO) << "Persisting Whitelist to disk.";
  if (store->Persist())
    SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal, "success");
  else
    SendSignalToChromium(chromium::kWhitelistChangeCompleteSignal, "failure");
  return FALSE;  // So that the event source that called this gets removed.
}

gboolean SessionManagerService::PersistStore(gpointer data) {
  PrefStore* store = static_cast<PrefStore*>(data);
  LOG(INFO) << "Persisting Store to disk.";
  if (store->Persist())
    SendSignalToChromium(chromium::kPropertyChangeCompleteSignal, "success");
  else
    SendSignalToChromium(chromium::kPropertyChangeCompleteSignal, "failure");
  return FALSE;  // So that the event source that called this gets removed.
}


///////////////////////////////////////////////////////////////////////////////
// Utility Methods

// This can probably be more efficient, if it needs to be.
// static
bool SessionManagerService::ValidateEmail(const string& email_address) {
  if (email_address.find_first_not_of(kLegalCharacters) != string::npos)
    return false;

  size_t at = email_address.find(kEmailSeparator);
  // it has NO @.
  if (at == string::npos)
    return false;

  // it has more than one @.
  if (email_address.find(kEmailSeparator, at+1) != string::npos)
    return false;

  return true;
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

void SessionManagerService::CleanupChildren(int timeout) {
  for (size_t i_child = 0; i_child < child_pids_.size(); ++i_child) {
    int child_pid = child_pids_[i_child];
    if (child_pid > 0) {
      system_->kill(child_pid, (session_started_ ? SIGTERM: SIGKILL));
      if (!system_->ChildIsGone(child_pid, timeout))
        system_->kill(child_pid, SIGABRT);
    }
  }
}

void SessionManagerService::SetGError(GError** error,
                                      ChromeOSLoginError code,
                                      const char* message) {
  g_set_error(error, CHROMEOS_LOGIN_ERROR, code, "Login error: %s", message);
}

// static
void SessionManagerService::SendSignalToChromium(const char* signal_name,
                                                 const char* payload) {
  chromeos::dbus::Proxy proxy(chromeos::dbus::GetSystemBusConnection(),
                              "/",
                              chromium::kChromiumInterface);
  if (!proxy) {
    LOG(ERROR) << "No proxy; can't signal chrome";
    return;
  }
  DBusMessage* signal = ::dbus_message_new_signal("/",
                                                  chromium::kChromiumInterface,
                                                  signal_name);
  if (payload) {
    dbus_message_append_args(signal,
                             DBUS_TYPE_STRING, &payload,
                             DBUS_TYPE_INVALID);
  }
  ::dbus_g_proxy_send(proxy.gproxy(), signal, NULL);
  ::dbus_message_unref(signal);
}

// static
std::vector<std::vector<std::string> > SessionManagerService::GetArgLists(
    std::vector<std::string> args) {
  std::vector<std::string> arg_list;
  std::vector<std::vector<std::string> > arg_lists;
  for (size_t i_arg = 0; i_arg < args.size(); ++i_arg) {
    if (args[i_arg] == "--") {
      if (arg_list.size()) {
        arg_lists.push_back(arg_list);
        arg_list.clear();
      }
    } else {
      arg_list.push_back(args[i_arg]);
    }
  }
  if (arg_list.size()) {
    arg_lists.push_back(arg_list);
  }
  return arg_lists;
}

}  // namespace login_manager
