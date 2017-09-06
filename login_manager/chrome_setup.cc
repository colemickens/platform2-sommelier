// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/chrome_setup.h"

#include <sys/stat.h>
#include <unistd.h>

#include <set>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/sha1.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <brillo/userdb_utils.h>
#include <chromeos-config/libcros_config/cros_config_interface.h>
#include <chromeos/ui/chromium_command_builder.h>
#include <chromeos/ui/util.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>

// IMPORTANT: If you want to check for the presence of a new USE flag within
// this file via UseFlagIsSet(), you need to add it to the IUSE list in the
// libchromeos-use-flags package's ebuild file. See docs/flags.md for more
// information about this file.

using chromeos::ui::ChromiumCommandBuilder;
using chromeos::ui::util::EnsureDirectoryExists;

namespace login_manager {

const char kWallpaperProperty[] = "wallpaper";

// These hashes are only being used temporarily till we can determine if a
// device is a Chromebox for Meetings or not from the Install Time attributes.
// TODO(rkc, pbos): Remove these and related code once crbug.com/706523 is
// fixed.
const char* kChromeboxForMeetingAppIdHashes[] = {
    "E703483CEF33DEC18B4B6DD84B5C776FB9182BDB",
    "A3BC37E2148AC4E99BE4B16AF9D42DD1E592BBBE",
    "1C93BD3CF875F4A73C0B2A163BB8FBDA8B8B3D80",
    "307E96539209F95A1A8740C713E6998A73657D96",
    "4F25792AF1AA7483936DE29C07806F203C7170A0",
    "BD8781D757D830FC2E85470A1B6E8A718B7EE0D9",
    "4AC2B6C63C6480D150DFDA13E4A5956EB1D0DDBB",
    "81986D4F846CEDDDB962643FA501D1780DD441BB",
};

namespace {

// Path to file containing developer-supplied modifications to Chrome's
// environment and command line. Passed to
// ChromiumCommandBuilder::ApplyUserConfig().
const char kChromeDevConfigPath[] = "/etc/chrome_dev.conf";

// Returns a base::FilePath corresponding to the DATA_DIR environment variable.
base::FilePath GetDataDir(ChromiumCommandBuilder* builder) {
  return base::FilePath(builder->ReadEnvVar("DATA_DIR"));
}

// Returns a base::FilePath corresponding to the subdirectory of DATA_DIR where
// user data is stored.
base::FilePath GetUserDir(ChromiumCommandBuilder* builder) {
  return base::FilePath(GetDataDir(builder).Append("user"));
}

// Called by AddUiFlags() to take a wallpaper flag type ("default" or "guest"
// or "child") and file type (e.g. "child", "default", "oem", "guest") and
// add the corresponding flags to |builder| if the files exist. Returns false
// if the files don't exist.
bool AddWallpaperFlags(
    ChromiumCommandBuilder* builder,
    const std::string& flag_type,
    const std::string& file_type,
    base::Callback<bool(const base::FilePath&)> path_exists) {
  const base::FilePath large_path(base::StringPrintf(
      "/usr/share/chromeos-assets/wallpaper/%s_large.jpg", file_type.c_str()));
  const base::FilePath small_path(base::StringPrintf(
      "/usr/share/chromeos-assets/wallpaper/%s_small.jpg", file_type.c_str()));
  if (!path_exists.Run(large_path) || !path_exists.Run(small_path)) {
    LOG(WARNING) << "Could not find both paths: " << large_path.MaybeAsASCII()
                 << " and " << small_path.MaybeAsASCII();
    return false;
  }

  builder->AddArg(base::StringPrintf("--%s-wallpaper-large=%s",
                                     flag_type.c_str(),
                                     large_path.value().c_str()));
  builder->AddArg(base::StringPrintf("--%s-wallpaper-small=%s",
                                     flag_type.c_str(),
                                     small_path.value().c_str()));
  return true;
}

// Adds ARC related flags.
void AddArcFlags(ChromiumCommandBuilder* builder,
                 std::set<std::string>* disallowed_params_out) {
  if (builder->UseFlagIsSet("arc") ||
      (builder->UseFlagIsSet("cheets") && builder->is_test_build())) {
    builder->AddArg("--arc-availability=officially-supported");
  } else if (builder->UseFlagIsSet("cheets")) {
    builder->AddArg("--arc-availability=installed");
  } else {
    // Don't pass ARC availability related flags in chrome_dev.conf to Chrome if
    // ARC is not installed at all.
    disallowed_params_out->insert("--arc-availability");
    disallowed_params_out->insert("--enable-arc");
    disallowed_params_out->insert("--arc-available");
    disallowed_params_out->insert("-arc-availability");
    disallowed_params_out->insert("-enable-arc");
    disallowed_params_out->insert("-arc-available");
  }

  if (builder->UseFlagIsSet("arc_oobe_optin"))
    builder->AddArg("--enable-arc-oobe-optin");
  if (builder->UseFlagIsSet("arc_always_start_with_no_play_store"))
    builder->AddArg("--arc-start-mode=always-start-with-no-play-store");
  if (builder->UseFlagIsSet("arc_transition_m_to_n"))
    builder->AddArg("--arc-transition-migration-required");
  if (builder->UseFlagIsSet("arc_only_start_after_login"))
    builder->AddArg("--arc-start-mode=only-start-after-login");
}

// Blatantly copied from //components/crx_file/id_util.cc.
// TODO(rkc): Remove when crbug.com/706523 is fixed.
std::string HashedIdInHex(const std::string& id) {
  const std::string id_hash = base::SHA1HashString(id);
  DCHECK_EQ(base::kSHA1Length, id_hash.length());
  return base::HexEncode(id_hash.c_str(), id_hash.length());
}

// Returns true if the ID matches any of the IDs of the kiosk apps run on
// Chromebox for Meetings.
bool IsChromeboxForMeetingsAppId(const std::string& id) {
  const std::string hash = HashedIdInHex(id);
  const char** end = kChromeboxForMeetingAppIdHashes +
                     arraysize(kChromeboxForMeetingAppIdHashes);
  return std::find(kChromeboxForMeetingAppIdHashes, end, hash) != end;
}

// Returns true if current device is enrolled as a Chromebox for Meetings.
bool IsEnrolledChromeboxForMeetings() {
  policy::PolicyProvider provider;
  if (!provider.Reload())
    return false;
  const policy::DevicePolicy& policy = provider.GetDevicePolicy();
  std::string kiosk_app_id;
  return policy.GetAutoLaunchedKioskAppId(&kiosk_app_id) &&
         IsChromeboxForMeetingsAppId(kiosk_app_id);
}

// Ensures that necessary directory exist with the correct permissions and sets
// related arguments and environment variables.
void CreateDirectories(ChromiumCommandBuilder* builder) {
  const uid_t uid = builder->uid();
  const gid_t gid = builder->gid();
  const uid_t kRootUid = 0;
  const gid_t kRootGid = 0;

  const base::FilePath data_dir = GetDataDir(builder);
  builder->AddArg("--user-data-dir=" + data_dir.value());

  const base::FilePath user_dir = GetUserDir(builder);
  CHECK(EnsureDirectoryExists(user_dir, uid, gid, 0755));
  // TODO(keescook): Remove Chrome's use of $HOME.
  builder->AddEnvVar("HOME", user_dir.value());

  // Old builds will have a profile dir that's owned by root; newer ones won't
  // have this directory at all.
  CHECK(EnsureDirectoryExists(data_dir.Append("Default"), uid, gid, 0755));

  // TODO(cmasone,derat): Stop using this directory and delete this code.
  const base::FilePath state_dir("/run/state");
  CHECK(base::DeleteFile(state_dir, true));
  CHECK(EnsureDirectoryExists(state_dir, kRootUid, kRootGid, 0710));

  // Create a directory where the session manager can store a copy of the user
  // policy key, that will be readable by the chrome process as chronos.
  const base::FilePath policy_dir("/run/user_policy");
  CHECK(base::DeleteFile(policy_dir, true));
  CHECK(EnsureDirectoryExists(policy_dir, kRootUid, gid, 0710));

  // Create a directory where the chrome process can store a reboot request so
  // that it persists across browser crashes but is always removed on reboot.
  CHECK(EnsureDirectoryExists(base::FilePath("/run/chrome"), uid, gid, 0700));

  // Ensure the existence of the directory in which the whitelist and other
  // ownership-related state will live. Yes, it should be owned by root. The
  // permissions are set such that the chronos user can see the content of known
  // files inside whitelist, but not anything else.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/lib/whitelist"), kRootUid, gid, 0710));

  // Create the directory where policies for extensions installed in
  // device-local accounts are cached. This data is read and written by chronos.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/device_local_account_component_policy"),
      uid,
      gid,
      0700));

  // Create the directory where external data referenced by policies is cached
  // for device-local accounts. This data is read and written by chronos.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/device_local_account_external_policy_data"),
      uid,
      gid,
      0700));

  // Create the directory where the AppPack extensions are cached.
  // These extensions are read and written by chronos.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/app_pack"), uid, gid, 0700));

  // Create the directory where extensions for device-local accounts are cached.
  // These extensions are read and written by chronos.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/device_local_account_extensions"),
      uid,
      gid,
      0700));

  // Create the directory where the Quirks Client can store downloaded
  // icc and other display profiles.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/display_profiles"), uid, gid, 0700));

  // Create the directory for shared installed extensions.
  // Shared extensions are validated at runtime by the browser.
  // These extensions are read and written by chronos.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/shared_extensions"), uid, gid, 0700));

  // Create the directory where policies for extensions installed in the
  // sign-in profile are cached. This data is read and written by chronos.
  CHECK(EnsureDirectoryExists(
      base::FilePath("/var/cache/signin_profile_component_policy"),
      uid,
      gid,
      0700));

  // Tell Chrome where to write logging messages before the user logs in.
  base::FilePath system_log_dir("/var/log/chrome");
  CHECK(EnsureDirectoryExists(system_log_dir, uid, gid, 0755));
  builder->AddEnvVar("CHROME_LOG_FILE",
                     system_log_dir.Append("chrome").value());

  // Log directory for the user session. Note that the user dir won't be mounted
  // until later (when the cryptohome is mounted), so we don't create
  // CHROMEOS_SESSION_LOG_DIR here.
  builder->AddEnvVar("CHROMEOS_SESSION_LOG_DIR",
                     user_dir.Append("log").value());

  // On devices with ARC++ camera HAL v3 Chrome needs to host the unix domain
  // named socket /run/camera/camera3.sock to provide the camera HAL Mojo
  // service to the system.
  if (base::PathExists(base::FilePath("/usr/bin/arc_camera3_service"))) {
    // The socket is created and listened on by Chrome, and receives connections
    // from the camera HAL v3 process and cameraserver process in Android
    // container which run as group arc-camera.  In addition, the camera HAL v3
    // process also hosts a unix domain named socket in /run/camera for the
    // sandboxed camera library process.  Thus the directory is created with
    // user chronos and group arc-camera with 0770 permission.
    gid_t arc_camera_gid;
    CHECK(brillo::userdb::GetGroupInfo("arc-camera", &arc_camera_gid));
    CHECK(EnsureDirectoryExists(
        base::FilePath("/run/camera"), uid, arc_camera_gid, 0770));
  }
}

// Creates crash-handling-related directories and adds related arguments.
void InitCrashHandling(ChromiumCommandBuilder* builder) {
  const base::FilePath user_dir = GetUserDir(builder);
  const uid_t uid = builder->uid();
  const gid_t gid = builder->gid();

  // Force Chrome minidumps that are sent to the crash server to also be written
  // locally. Chrome creates these files in
  // ~/.config/google-chrome/Crash Reports/.
  const base::FilePath stateful_etc("/mnt/stateful_partition/etc");
  if (base::PathExists(stateful_etc.Append("enable_chromium_minidumps"))) {
    builder->AddEnvVar("CHROME_HEADLESS", "1");
    const base::FilePath reports_dir(
        user_dir.Append(".config/google-chrome/Crash Reports"));
    if (!base::PathExists(reports_dir)) {
      base::FilePath minidump_dir("/var/minidumps");
      EnsureDirectoryExists(minidump_dir, uid, gid, 0700);
      EnsureDirectoryExists(reports_dir.DirName(), uid, gid, 0700);
      base::CreateSymbolicLink(minidump_dir, reports_dir);
    }
  }

  // Enable gathering of core dumps via a file in the stateful partition so it
  // can be enabled post-build.
  if (base::PathExists(stateful_etc.Append("enable_chromium_coredumps")))
    builder->EnableCoreDumps();
}

// Adds system-related flags to the command line.
void AddSystemFlags(ChromiumCommandBuilder* builder) {
  const base::FilePath data_dir = GetDataDir(builder);

  // We need to delete these files as Chrome may have left them around from its
  // prior run (if it crashed).
  base::DeleteFile(data_dir.Append("SingletonLock"), false);
  base::DeleteFile(data_dir.Append("SingletonSocket"), false);

  builder->AddArg("--max-unused-resource-memory-usage-percentage=5");

  // For Caroline we are going to experiment with a higher number of renderer
  // processes to allow the tab discarder to be more effective.
  // The 4GB RAM default is 34 see  https://crbug.com/741231
  if (builder->UseFlagIsSet("caroline")) {
    builder->AddArg("--renderer-process-limit=100");
  }

  // On developer systems, set a flag to let the browser know.
  if (builder->is_developer_end_user())
    builder->AddArg("--system-developer-mode");
}

// Adds UI-related flags to the command line.
void AddUiFlags(ChromiumCommandBuilder* builder,
                brillo::CrosConfigInterface* cros_config) {
  const base::FilePath data_dir = GetDataDir(builder);

  // Force OOBE on test images that have requested it.
  if (base::PathExists(base::FilePath("/root/.test_repeat_oobe"))) {
    base::DeleteFile(data_dir.Append(".oobe_completed"), false);
    base::DeleteFile(data_dir.Append("Local State"), false);
  }

  if (builder->UseFlagIsSet("eve"))
    builder->AddArg("--force-clamshell-power-button");
  if (builder->UseFlagIsSet("kevin")) {
    // TODO(jdufault): Remove this once quick unlock launches on all boards.
    builder->AddFeatureEnableOverride("QuickUnlockPin");
  }
  if (builder->UseFlagIsSet("veyron_minnie"))
    builder->AddArg("--enable-hardware-overlays=single-fullscreen");

  // Chromebox for meetings devices need to start with this flag till
  // crbug.com/653531 gets fixed. TODO(pbos): Remove this once this feature is
  // enabled by default.
  if (builder->UseFlagIsSet("cfm_enabled_device")) {
    if (IsEnrolledChromeboxForMeetings()) {
      builder->AddArg("--enable-blink-features=MediaStreamTrackContentHint");
    }
    if (builder->UseFlagIsSet("screenshare_sw_codec")) {
      builder->AddArg("--enable-features=WebRtcScreenshareSwEncoding");
    }
  }

  if (builder->UseFlagIsSet("rialto")) {
    if (builder->UseFlagIsSet("rialto_enterprise_enrollment")) {
      // The new mode of rialto operation. See crbug.com/574923
      builder->AddArg("--login-manager");
      builder->AddArg("--enterprise-enable-zero-touch-enrollment=hands-off");
      builder->AddArg("--disable-machine-cert-request");
      builder->AddArg("--cellular-first");
      builder->AddArg(
          "--app-mode-oem-manifest=/etc/rialto_overlay_oem_manifest.json");
      builder->AddArg("--log-level=0");
      builder->AddArg("--disable-logging-redirect");
    } else {
      // The current mode of rialto operation.
      builder->AddArg("--disable-demo-mode");
      builder->AddArg("--kiosk");
      builder->AddArg("--login-user=chronos");
      builder->AddArg("--ash-hide-notifications-for-factory");
      builder->AddArg("--cellular-first");
      builder->AddArg("--load-and-launch-app=/usr/share/app_shell/apps/rialto");
      builder->AddArg("--enable-logging=stderr");
      builder->AddArg("--log-level=0");
      builder->AddArg("--enable-ble-advertising-in-apps");
      builder->AddArg("about:blank");
    }
  } else {
    builder->AddArg("--login-manager");
  }
  builder->AddArg("--login-profile=user");

  if (builder->UseFlagIsSet("natural_scroll_default"))
    builder->AddArg("--enable-natural-scroll-default");
  if (!builder->UseFlagIsSet("legacy_keyboard"))
    builder->AddArg("--has-chromeos-keyboard");
  if (builder->UseFlagIsSet("has_diamond_key"))
    builder->AddArg("--has-chromeos-diamond-key");
  if (builder->UseFlagIsSet("legacy_power_button"))
    builder->AddArg("--aura-legacy-power-button");
  if (builder->UseFlagIsSet("touchview"))
    builder->AddArg("--enable-touchview");
  if (builder->UseFlagIsSet("internal_stylus"))
    builder->AddArg("--has-internal-stylus");

  if (builder->UseFlagIsSet("voice_interaction"))
    builder->AddArg("--enable-voice-interaction");

  if (builder->UseFlagIsSet("background_blur"))
    builder->AddFeatureEnableOverride("EnableBackgroundBlur");

  SetUpWallpaperFlags(builder, cros_config, base::Bind(base::PathExists));

  // TODO(yongjaek): Remove the following flag when the kiosk mode app is ready
  // at crbug.com/309806.
  if (builder->UseFlagIsSet("moblab"))
    builder->AddArg("--disable-demo-mode");

  if (builder->UseFlagIsSet("allow_consumer_kiosk"))
    builder->AddArg("--enable-consumer-kiosk");
}

// Adds enterprise-related flags to the command line.
void AddEnterpriseFlags(ChromiumCommandBuilder* builder) {
  builder->AddArg("--enterprise-enrollment-initial-modulus=15");
  builder->AddArg("--enterprise-enrollment-modulus-limit=19");

  // This flag is only used in M49 to enable bootstrapping for ChromeBit. All
  // Chrome OS devices will be eligible for bootstrapping starting from M50.
  // TODO(xdai): Remove this after it's cherry-picked in M49.
  if (builder->UseFlagIsSet("veyron_mickey"))
    builder->AddArg("--oobe-bootstrapping-slave");
}

// Adds patterns to the --vmodule flag.
void AddVmodulePatterns(ChromiumCommandBuilder* builder) {
  // There has been a steady supply of bug reports about screen locking. These
  // messages are useful for determining what happened within feedback reports.
  // See crbug.com/452599.
  builder->AddVmodulePattern("screen_locker=2");
  builder->AddVmodulePattern("webui_screen_locker=2");
  builder->AddVmodulePattern("lock_state_controller=2");
  builder->AddVmodulePattern("webui_login_view=2");
  builder->AddVmodulePattern("power_button_observer=2");

  // Turn on logging about external displays being connected and disconnected.
  // Different behavior is seen from different displays and these messages are
  // used to determine what happened within feedback reports.
  builder->AddVmodulePattern("*/ui/display/manager/chromeos/*=1");

  // Turn on basic logging for Ozone platform implementations.
  builder->AddVmodulePattern("*/ui/ozone/*=1");

  // Turn on plugin loading failure logging for crbug.com/314301.
  builder->AddVmodulePattern("*zygote*=1");
  builder->AddVmodulePattern("*plugin*=2");

  // Needed for investigating auto-enrollment issues.
  // TODO(tnagel): Remove after switching to device_event_log:
  // http://crbug.com/636184
  builder->AddVmodulePattern("auto_enrollment_controller=1");

  // There is a mysterious offline login failure. Turn on logging on
  // the login code path.
  // TODO(xiyuan): Remove after http://crbug.com/547857 is resolved.
  builder->AddVmodulePattern("*chromeos/login/*=1");

  // Used to evaluate different experimental flag settings for
  // http://crbug.com/736936. Nothing is logged unless the flags are set via
  // chrome://flags.
  // TODO(derat): Remove after http://crbug.com/736936 is closed.
  builder->AddVmodulePattern("tablet_power_button_controller=1");

  if (builder->UseFlagIsSet("cheets"))
    builder->AddVmodulePattern("*arc/*=1");
}

}  // namespace

void SetUpWallpaperFlags(
    ChromiumCommandBuilder* builder,
    brillo::CrosConfigInterface* cros_config,
    base::Callback<bool(const base::FilePath&)> path_exists) {
  std::string model;
  // Use the configuration if available, then fall back to oem and default.
  if (cros_config && cros_config->GetString("/", kWallpaperProperty, &model) &&
      AddWallpaperFlags(builder, "default", model, path_exists)) {
    AddWallpaperFlags(builder, "child", "child", path_exists);
  } else if (AddWallpaperFlags(builder, "default", "oem", path_exists)) {
    builder->AddArg("--default-wallpaper-is-oem");
  } else {
    AddWallpaperFlags(builder, "default", "default", path_exists);
    AddWallpaperFlags(builder, "child", "child", path_exists);
  }
  AddWallpaperFlags(builder, "guest", "guest", path_exists);
}

void PerformChromeSetup(brillo::CrosConfigInterface* cros_config,
                        bool* is_developer_end_user_out,
                        std::map<std::string, std::string>* env_vars_out,
                        std::vector<std::string>* args_out,
                        uid_t* uid_out) {
  DCHECK(env_vars_out);
  DCHECK(args_out);
  DCHECK(uid_out);

  ChromiumCommandBuilder builder;
  std::set<std::string> disallowed_prefixes;
  CHECK(builder.Init());
  CHECK(builder.SetUpChromium());

  // Please add new code to the most-appropriate helper function instead of
  // putting it here. Things that to all Chromium-derived binaries (e.g.
  // app_shell, content_shell, etc.) rather than just to Chrome belong in the
  // ChromiumCommandBuilder class instead.
  CreateDirectories(&builder);
  InitCrashHandling(&builder);
  AddSystemFlags(&builder);
  AddUiFlags(&builder, cros_config);
  AddArcFlags(&builder, &disallowed_prefixes);
  AddEnterpriseFlags(&builder);
  AddVmodulePatterns(&builder);

  // Apply any modifications requested by the developer.
  if (builder.is_developer_end_user()) {
    builder.ApplyUserConfig(base::FilePath(kChromeDevConfigPath),
                            disallowed_prefixes);
  }

  *is_developer_end_user_out = builder.is_developer_end_user();
  *env_vars_out = builder.environment_variables();
  *args_out = builder.arguments();
  *uid_out = builder.uid();

  // Do not add code here. Potentially-expensive work should be done between
  // StartServer() and WaitForServer().
}

}  // namespace login_manager
