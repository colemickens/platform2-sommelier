/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <linux/media.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <brillo/syslog_logging.h>

namespace {

const char kSysfsV4lClassRoot[] = "/sys/class/video4linux";
const char kVendorIdPath[] = "device/vendor_id";
const std::vector<std::string> kArgsPattern = {"modules", "list"};

struct camera {
  std::string name;
  std::string vendor_id;

  camera(const std::string& name_, const std::string& vendor_id_)
      : name(name_), vendor_id(vendor_id_) {}
};

class CameraTool {
  typedef std::vector<struct camera> CameraVector;

 public:
  void PrintCameras(void) {
    const CameraVector& cameras = GetPlatformCameras();

    if (cameras.empty()) {
      std::cout << "No cameras detected in the system." << std::endl;
      return;
    }

    std::cout << std::setw(16) << "Name"
              << " | "
              << "Vendor ID" << std::endl;
    for (const auto& camera : cameras)
      std::cout << std::setw(16) << camera.name << " | " << camera.vendor_id
                << std::endl;
  }

 private:
  void ProbeSensorSubdev(struct media_entity_desc* desc,
                         const base::FilePath& path) {
    const base::FilePath& vendor_id_path = path.Append(kVendorIdPath);
    std::string vendor_id = "-1";

    if (!base::ReadFileToStringWithMaxSize(vendor_id_path, &vendor_id, 64))
      LOG(ERROR) << "Failed to read vendor ID for sensor '" << desc->name
                 << "'";

    base::TrimWhitespaceASCII(vendor_id, base::TRIM_ALL, &vendor_id);
    platform_cameras_.emplace_back(desc->name, vendor_id);
  }

  base::FilePath FindSubdevSysfsByDevId(int major, int minor) {
    base::FileEnumerator dev_enum(base::FilePath(kSysfsV4lClassRoot), false,
                                  base::FileEnumerator::DIRECTORIES,
                                  "v4l-subdev*");
    for (base::FilePath name = dev_enum.Next(); !name.empty();
         name = dev_enum.Next()) {
      base::FilePath dev_path = name.Append("dev");
      std::string dev_id("255:255");
      if (!base::ReadFileToStringWithMaxSize(dev_path, &dev_id,
                                             dev_id.size())) {
        LOG(ERROR) << "Failed to read device ID of '" << dev_path.value()
                   << "' from sysfs";
        continue;
      }
      base::TrimWhitespaceASCII(dev_id, base::TRIM_ALL, &dev_id);

      std::ostringstream stream;
      stream << major << ":" << minor;
      if (dev_id == stream.str())
        return name;
    }

    return base::FilePath();
  }

  void ProbeMediaController(int media_fd) {
    struct media_entity_desc desc;

    for (desc.id = MEDIA_ENT_ID_FLAG_NEXT;
         !ioctl(media_fd, MEDIA_IOC_ENUM_ENTITIES, &desc);
         desc.id |= MEDIA_ENT_ID_FLAG_NEXT) {
      if (desc.type != MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)
        continue;

      const base::FilePath& path =
          FindSubdevSysfsByDevId(desc.dev.major, desc.dev.minor);
      if (path.empty()) {
        LOG(ERROR) << "v4l-subdev node for sensor '" << desc.name
                   << "' not found";
        continue;
      }

      LOG(INFO) << "Probing sensor '" << desc.name << "' ("
                << path.BaseName().value() << ")";
      ProbeSensorSubdev(&desc, path);
    }
  }

  void AddV4l2Cameras(void) {
    base::FileEnumerator dev_enum(base::FilePath("/dev"), false,
                                  base::FileEnumerator::FILES, "media*");
    for (base::FilePath name = dev_enum.Next(); !name.empty();
         name = dev_enum.Next()) {
      int fd = open(name.value().c_str(), O_RDWR);
      if (fd < 0) {
        LOG(ERROR) << "Failed to open '" << name.value() << "'";
        continue;
      }

      LOG(INFO) << "Probing media device '" << name.value() << "'";
      ProbeMediaController(fd);
      close(fd);
    }
  }

  const CameraVector& GetPlatformCameras() {
    if (platform_cameras_.empty())
      AddV4l2Cameras();

    return platform_cameras_;
  }

  CameraVector platform_cameras_;
};

bool StringEqualsCaseInsensitiveASCII(const std::string& a,
                                      const std::string& b) {
  return base::EqualsCaseInsensitiveASCII(a, b);
}

}  // namespace

int main(int argc, char* argv[]) {
  // Init CommandLine for InitLogging.
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  base::AtExitManager at_exit_manager;

  int log_flags = brillo::kLogToSyslog;
  if (cl->HasSwitch("foreground")) {
    log_flags |= brillo::kLogToStderr;
  }
  brillo::InitLog(log_flags);

  // FIXME: Currently only "modules list" command is supported
  const std::vector<std::string>& args = cl->GetArgs();
  if (cl->GetArgs().empty() ||
      !std::equal(kArgsPattern.begin(), kArgsPattern.end(), args.begin(),
                  StringEqualsCaseInsensitiveASCII)) {
    LOG(ERROR) << "Invalid command.";
    LOG(ERROR) << "Try following supported commands:";
    LOG(ERROR) << "  modules - operations on camera modules";
    LOG(ERROR) << "    list - print available modules";
    return 1;
  }

  CameraTool tool;
  tool.PrintCameras();

  return 0;
}
