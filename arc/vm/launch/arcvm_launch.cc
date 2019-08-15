// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/statvfs.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/at_exit.h>
#include <base/environment.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/optional.h>
#include <base/run_loop.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/errors/error.h>
#include <brillo/key_value_store.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <vboot/crossystem.h>
#include <vm_concierge/proto_bindings/service.pb.h>

namespace {

constexpr char kHomeDirectory[] = "/home";
constexpr char kKernelPath[] = "/opt/google/vms/android/vmlinux";
constexpr char kRootFsPath[] = "/opt/google/vms/android/system.raw.img";
constexpr char kVendorImagePath[] = "/opt/google/vms/android/vendor.raw.img";

constexpr auto DEFAULT_TIMEOUT = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;

bool IsHostRootfsWritable() {
  struct statvfs buf;
  if (statvfs("/", &buf) < 0) {
    PLOG(ERROR) << "statvfs() failed";
    return false;
  }
  const bool rw = !(buf.f_flag & ST_RDONLY);
  LOG(INFO) << "Host's rootfs is " << (rw ? "rw" : "ro");
  return rw;
}

}  // namespace

// TODO(cmtm): This should be replaced with chromeos-dbus-bindings or a
// concierge client library whenever they are ready.
class ConciergeClient {
 public:
  explicit ConciergeClient(scoped_refptr<dbus::Bus> bus)
      : bus_(bus),
        proxy_(bus->GetObjectProxy(
            vm_tools::concierge::kVmConciergeServiceName,
            dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath))) {}

  std::string CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request) {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kCreateDiskImageMethod);
    dbus::MessageWriter writer(&method_call);

    CHECK(writer.AppendProtoAsArrayOfBytes(request))
        << "Failed to encode CreateDiskImageRequest protobuf";

    auto dbus_response =
        proxy_->CallMethodAndBlock(&method_call, DEFAULT_TIMEOUT);

    CHECK(dbus_response) << "Failed to send dbus message to concierge service";

    dbus::MessageReader reader(dbus_response.get());
    vm_tools::concierge::CreateDiskImageResponse response;
    CHECK(reader.PopArrayOfBytesAsProto(&response))
        << "Failed to parse response protobuf";

    if (response.status() == vm_tools::concierge::DISK_STATUS_EXISTS) {
      LOG(INFO) << "Disk image already exists: " << response.disk_path();
    } else if (response.status() == vm_tools::concierge::DISK_STATUS_CREATED) {
      LOG(INFO) << "Disk image created: " << response.disk_path();
    } else {
      LOG(FATAL) << "Failed to create disk image: "
                 << response.failure_reason();
    }
    return response.disk_path();
  }

  void StartArcVm(const vm_tools::concierge::StartArcVmRequest& request) {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStartArcVmMethod);
    dbus::MessageWriter writer(&method_call);
    CHECK(writer.AppendProtoAsArrayOfBytes(request))
        << "Failed to encode StartVmRequest protobuf";

    std::unique_ptr<dbus::Response> dbus_response =
        proxy_->CallMethodAndBlock(&method_call, DEFAULT_TIMEOUT);
    CHECK(dbus_response) << "Failed to send dbus message to concierge service";
    dbus::MessageReader reader(dbus_response.get());
    vm_tools::concierge::StartVmResponse response;
    CHECK(reader.PopArrayOfBytesAsProto(&response))
        << "Failed to parse response protobuf";

    CHECK_NE(response.status(), vm_tools::concierge::VM_STATUS_FAILURE);
    CHECK_NE(response.status(), vm_tools::concierge::VM_STATUS_UNKNOWN);
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  // |proxy_| doesn't own the ObjectProxy.
  dbus::ObjectProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(ConciergeClient);
};

class ChromeFeaturesServiceClient {
 public:
  explicit ChromeFeaturesServiceClient(scoped_refptr<dbus::Bus> bus)
      : bus_(bus),
        proxy_(bus->GetObjectProxy(
            chromeos::kChromeFeaturesServiceName,
            dbus::ObjectPath(chromeos::kChromeFeaturesServicePath))) {}

  bool IsEnabled(std::string feature_name) {
    using brillo::dbus_utils::CallMethodAndBlock;
    using brillo::dbus_utils::ExtractMethodCallResults;

    brillo::ErrorPtr error;
    auto resp = CallMethodAndBlock(
        proxy_, chromeos::kChromeFeaturesServiceInterface,
        chromeos::kChromeFeaturesServiceIsFeatureEnabledMethod, &error,
        std::move(feature_name));
    bool is_enabled = false;
    CHECK(resp && ExtractMethodCallResults(resp.get(), &error, &is_enabled))
        << "IsFeatureEnabled DBus method call failed: " << error->GetMessage();
    return is_enabled;
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  // |proxy_| doesn't own the ObjectProxy.
  dbus::ObjectProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFeaturesServiceClient);
};

std::string GetReleaseChannel() {
  const std::set<std::string> kChannels = {"beta",    "canary", "dev",
                                           "dogfood", "stable", "testimage"};
  const std::string kUnknown = "unknown";

  std::string value;
  if (!base::SysInfo::GetLsbReleaseValue("CHROMEOS_RELEASE_TRACK", &value)) {
    LOG(ERROR) << "Could not load lsb-release";
    return kUnknown;
  }

  auto pieces = base::SplitString(value, "-", base::KEEP_WHITESPACE,
                                  base::SPLIT_WANT_ALL);

  if (pieces.size() != 2 || pieces[1] != "channel") {
    LOG(ERROR) << "Misformatted CHROMEOS_RELEASE_TRACK value in lsb-release";
    return kUnknown;
  }
  if (kChannels.find(pieces[0]) == kChannels.end()) {
    LOG(WARNING) << "Unknown ChromeOS channel: \"" << pieces[0] << "\"";
    return kUnknown;
  }

  return pieces[0];
}
vm_tools::concierge::CreateDiskImageRequest CreateArcDiskRequest(
    const std::string& user_id) {
  int64_t free_disk_bytes =
      base::SysInfo::AmountOfFreeDiskSpace(base::FilePath(kHomeDirectory));
  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(user_id);
  request.set_disk_path("arcvm");
  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  // The logical size of the new disk image, in bytes.
  request.set_disk_size(free_disk_bytes / 2);

  return request;
}

std::string MonotonicTimestamp() {
  struct timespec ts;
  PCHECK(clock_gettime(CLOCK_BOOTTIME, &ts) == 0);
  int64_t time = ts.tv_sec * base::Time::kNanosecondsPerSecond + ts.tv_nsec;
  return std::to_string(time);
}

std::vector<std::string> GenerateKernelCmdline(
    scoped_refptr<dbus::Bus> bus,
    const std::string& lcd_density,
    const base::Optional<std::string>& play_store_auto_update) {
  using base::StringPrintf;

  int is_dev_mode = VbGetSystemPropertyInt("cros_debug");
  CHECK_NE(is_dev_mode, -1);
  int is_inside_vm = VbGetSystemPropertyInt("inside_vm");
  CHECK_NE(is_inside_vm, -1);

  std::string release_channel = GetReleaseChannel();
  bool stable_or_beta =
      release_channel == "stable" || release_channel == "beta";

  ChromeFeaturesServiceClient features(bus);

  std::vector<std::string> result = {
      "androidboot.hardware=bertha",
      "androidboot.container=1",
      // TODO(b/139480143): when ArcNativeBridgeExperiment is enabled, switch
      // to ndk_translation.
      "androidboot.native_bridge=libhoudini.so",
      StringPrintf("androidboot.dev_mode=%d", is_dev_mode),
      StringPrintf("androidboot.disable_runas=%d", !is_dev_mode),
      StringPrintf("androidboot.vm=%d", is_inside_vm),
      // TODO(cmtm): get this from arc-setup config or equivalent
      "androidboot.debuggable=1",
      "androidboot.lcd_density=" + lcd_density,
      StringPrintf("androidboot.arc_file_picker=%d",
                   features.IsEnabled("ArcFilePickerExperiment")),
      StringPrintf(
          "androidboot.arc_custom_tabs=%d",
          features.IsEnabled("ArcCustomTabsExperiment") && !stable_or_beta),
      StringPrintf(
          "androidboot.arc_print_spooler=%d",
          features.IsEnabled("ArcPrintSpoolerExperiment") && !stable_or_beta),
      "androidboot.chromeos_channel=" + GetReleaseChannel(),
      "androidboot.boottime_offset=" + MonotonicTimestamp(),
      // TODO(cmtm): remove this once arcvm supports SELinux
      "androidboot.selinux=permissive",
  };

  if (play_store_auto_update) {
    result.push_back("androidboot.play_store_auto_update=" +
                     *play_store_auto_update);
  }

  // TODO(cmtm): include command line parameters from arcbootcontinue
  return result;
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    const std::string& user_id_hash,
    const std::string& disk_path,
    std::vector<std::string> kernel_cmdline) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name("arcvm");
  request.set_owner_id(user_id_hash);

  request.add_params("root=/dev/vda");
  if (IsHostRootfsWritable())
    request.add_params("rw");
  request.add_params("init=/init");
  for (auto& entry : kernel_cmdline)
    request.add_params(std::move(entry));

  vm_tools::concierge::VirtualMachineSpec* vm = request.mutable_vm();
  vm->set_kernel(kKernelPath);
  // Add / as /dev/vda.
  vm->set_rootfs(kRootFsPath);

  // Add /data as /dev/vdb.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(disk_path);
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(true);
  // Add /vendor as /dev/vdc.
  disk_image = request.add_disks();
  disk_image->set_path(kVendorImagePath);
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);

  return request;
}

struct EnvParameters {
  EnvParameters() {
    auto env = base::Environment::Create();
    CHECK(env->GetVar("ARC_LCD_DENSITY", &lcd_density));
    CHECK(env->GetVar("USER_ID_HASH", &user_id_hash));
    if (env->HasVar("PLAY_STORE_AUTO_UPDATE")) {
      play_store_auto_update.emplace();
      CHECK(env->GetVar("PLAY_STORE_AUTO_UPDATE", &*play_store_auto_update));
    }
  }
  std::string lcd_density;
  std::string user_id_hash;
  base::Optional<std::string> play_store_auto_update;
};

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::MessageLoopForIO message_loop;

  brillo::OpenLog("arcvm-launch", true /*log_pid*/);

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);

  EnvParameters env;

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(std::move(options)));
  CHECK(bus->Connect());

  ConciergeClient concierge_client(bus);

  auto disk_request = CreateArcDiskRequest(env.user_id_hash);
  auto disk_path = concierge_client.CreateDiskImage(disk_request);
  auto kernel_cmdline =
      GenerateKernelCmdline(bus, env.lcd_density, env.play_store_auto_update);
  auto start_request = CreateStartArcVmRequest(
      env.user_id_hash, std::move(disk_path), std::move(kernel_cmdline));
  concierge_client.StartArcVm(start_request);

  return 0;
}
