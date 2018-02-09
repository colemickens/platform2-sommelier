/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/v4l2_camera_device.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include <limits>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/stringprintf.h>
#include <base/timer/elapsed_timer.h>

#include "cros-camera/common.h"
#include "hal/usb/camera_characteristics.h"

namespace arc {

// USB VID and PID are both 4 bytes long.
static const size_t kVidPidSize = 4;
// /sys/class/video4linux/video{N}/device is a symlink to the corresponding
// USB device info directory.
static const char kVidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idVendor";
static const char kPidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idProduct";

// Allowed camera devices.
static const char kAllowedCameraPrefix[] = "/dev/camera-internal";
static const char kAllowedVideoPrefix[] = "/dev/video";

static bool ReadIdFile(const std::string& path, std::string* id) {
  char id_buf[kVidPidSize];
  FILE* file = fopen(path.c_str(), "rb");
  if (!file)
    return false;
  const bool success = fread(id_buf, kVidPidSize, 1, file) == 1;
  fclose(file);
  if (!success)
    return false;
  id->append(id_buf, kVidPidSize);
  return true;
}

V4L2CameraDevice::V4L2CameraDevice()
    : stream_on_(false), device_info_(DeviceInfo()) {}

V4L2CameraDevice::V4L2CameraDevice(const DeviceInfo& device_info)
    : stream_on_(false), device_info_(device_info) {}

V4L2CameraDevice::~V4L2CameraDevice() {
  device_fd_.reset();
}

int V4L2CameraDevice::Connect(const std::string& device_path) {
  VLOGF(1) << "Connecting device path: " << device_path;
  base::AutoLock l(lock_);
  if (device_path.compare(0, strlen(kAllowedCameraPrefix),
                          kAllowedCameraPrefix) &&
      device_path.compare(0, strlen(kAllowedVideoPrefix),
                          kAllowedVideoPrefix)) {
    LOGF(ERROR) << "Invalid device path " << device_path;
    return -EINVAL;
  }
  if (device_fd_.is_valid()) {
    LOGF(ERROR) << "A camera device is opened (" << device_fd_.get()
                << "). Please close it first";
    return -EIO;
  }

  // If device path is /dev/video*, it means the device is an external camera.
  // We have to glob all video devices again since the device number may be
  // changed after suspend/resume.
  std::string correct_device_path = device_path;
  if (!device_path.compare(0, strlen(kAllowedVideoPrefix),
                           kAllowedVideoPrefix)) {
    std::pair<std::string, DeviceInfo> external_camera = FindExternalCamera();
    if (external_camera.first != "") {
      correct_device_path = external_camera.second.device_path;
    }
  }

  // Since device node may be changed after suspend/resume, we allow to use
  // symbolic link to access device.
  device_fd_.reset(RetryDeviceOpen(correct_device_path, O_RDWR));
  if (!device_fd_.is_valid()) {
    LOGF(ERROR) << "Failed to open " << device_path << " : " << strerror(errno);
    return -errno;
  }

  v4l2_capability cap = {};
  if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_QUERYCAP, &cap)) != 0) {
    LOGF(ERROR) << "VIDIOC_QUERYCAP fail: " << strerror(errno);
    device_fd_.reset();
    return -errno;
  }

  // TODO(henryhsu): Add MPLANE support.
  if (!((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT))) {
    LOGF(ERROR) << "This is not a V4L2 video capture device";
    device_fd_.reset();
    return -EIO;
  }

  // Get and set format here is used to prevent multiple camera using.
  // UVC driver will acquire lock in VIDIOC_S_FMT and VIDIOC_S_SMT will fail if
  // the camera is being used by a user. The second user will fail in Connect()
  // instead of StreamOn(). Usually apps show better error message if camera
  // open fails. If start preview fails, some apps do not handle it well.
  int ret;
  v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ret = TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_G_FMT, &fmt));
  if (ret < 0) {
    LOGF(ERROR) << "Unable to G_FMT: " << strerror(errno);
    return -errno;
  }
  ret = TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_S_FMT, &fmt));
  if (ret < 0) {
    LOGF(WARNING) << "Unable to S_FMT: " << strerror(errno)
                  << ", maybe camera is being used by another app.";
    return -errno;
  }
  return 0;
}

void V4L2CameraDevice::Disconnect() {
  base::AutoLock l(lock_);
  stream_on_ = false;
  device_fd_.reset();
  buffers_at_client_.clear();
}

int V4L2CameraDevice::StreamOn(uint32_t width,
                               uint32_t height,
                               uint32_t pixel_format,
                               float frame_rate,
                               std::vector<base::ScopedFD>* fds,
                               uint32_t* buffer_size) {
  base::AutoLock l(lock_);
  if (!device_fd_.is_valid()) {
    LOGF(ERROR) << "Device is not opened";
    return -ENODEV;
  }
  if (stream_on_) {
    LOGF(ERROR) << "Device has stream already started";
    return -EIO;
  }

  // Some drivers use rational time per frame instead of float frame rate, this
  // constant k is used to convert between both: A fps -> [k/k*A] seconds/frame.
  const int kFrameRatePrecision = 10000;
  int ret;
  v4l2_format fmt = {};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = width;
  fmt.fmt.pix.height = height;
  fmt.fmt.pix.pixelformat = pixel_format;
  ret = TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_S_FMT, &fmt));
  if (ret < 0) {
    LOGF(ERROR) << "Unable to S_FMT: " << strerror(errno);
    return -errno;
  }
  VLOGF(1) << "Actual width: " << fmt.fmt.pix.width
           << ", height: " << fmt.fmt.pix.height
           << ", pixelformat: " << std::hex << fmt.fmt.pix.pixelformat;

  if (width != fmt.fmt.pix.width || height != fmt.fmt.pix.height ||
      pixel_format != fmt.fmt.pix.pixelformat) {
    LOGF(ERROR) << "Unsupported format: width " << width << ", height "
                << height << ", pixelformat " << pixel_format;
    return -EINVAL;
  }

  // Set capture framerate in the form of capture interval.
  v4l2_streamparm streamparm = {};
  streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  // The following line checks that the driver knows about framerate get/set.
  if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_G_PARM, &streamparm)) >=
      0) {
    // Now check if the device is able to accept a capture framerate set.
    if (streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
      // |frame_rate| is float, approximate by a fraction.
      streamparm.parm.capture.timeperframe.numerator = kFrameRatePrecision;
      streamparm.parm.capture.timeperframe.denominator =
          (frame_rate * kFrameRatePrecision);

      if (TEMP_FAILURE_RETRY(
              ioctl(device_fd_.get(), VIDIOC_S_PARM, &streamparm)) < 0) {
        LOGF(ERROR) << "Failed to set camera framerate";
        return -EIO;
      }
      VLOGF(1) << "Actual camera driver framerate: "
               << streamparm.parm.capture.timeperframe.denominator << "/"
               << streamparm.parm.capture.timeperframe.numerator;
    }
  }
  float fps = streamparm.parm.capture.timeperframe.denominator /
              streamparm.parm.capture.timeperframe.numerator;
  if (std::fabs(fps - frame_rate) > std::numeric_limits<float>::epsilon()) {
    LOGF(ERROR) << "Unsupported frame rate " << frame_rate;
    return -EINVAL;
  }
  *buffer_size = fmt.fmt.pix.sizeimage;
  VLOGF(1) << "Buffer size: " << *buffer_size;

  // Set power line frequency
  ret = SetPowerLineFrequency(device_info_.power_line_frequency);
  if (ret < 0) {
    return -EINVAL;
  }

  v4l2_requestbuffers req_buffers;
  memset(&req_buffers, 0, sizeof(req_buffers));
  req_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req_buffers.memory = V4L2_MEMORY_MMAP;
  req_buffers.count = kNumVideoBuffers;
  if (TEMP_FAILURE_RETRY(
          ioctl(device_fd_.get(), VIDIOC_REQBUFS, &req_buffers)) < 0) {
    LOGF(ERROR) << "REQBUFS fails: " << strerror(errno);
    return -errno;
  }
  VLOGF(1) << "Requested buffer number: " << req_buffers.count;

  buffers_at_client_.resize(req_buffers.count);
  std::vector<base::ScopedFD> temp_fds;
  for (uint32_t i = 0; i < req_buffers.count; i++) {
    v4l2_exportbuffer expbuf;
    memset(&expbuf, 0, sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = i;
    if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_EXPBUF, &expbuf)) <
        0) {
      LOGF(ERROR) << "EXPBUF (" << i << ") fails: " << strerror(errno);
      return -errno;
    }
    VLOGF(1) << "Exported frame buffer fd: " << expbuf.fd;
    temp_fds.push_back(base::ScopedFD(expbuf.fd));
    buffers_at_client_[i] = false;

    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.index = i;
    buffer.memory = V4L2_MEMORY_MMAP;

    if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_QBUF, &buffer)) < 0) {
      LOGF(ERROR) << "QBUF (" << i << ") fails: " << strerror(errno);
      return -errno;
    }
  }

  v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (TEMP_FAILURE_RETRY(
          ioctl(device_fd_.get(), VIDIOC_STREAMON, &capture_type)) < 0) {
    LOGF(ERROR) << "STREAMON fails: " << strerror(errno);
    return -errno;
  }

  for (size_t i = 0; i < temp_fds.size(); i++) {
    fds->push_back(std::move(temp_fds[i]));
  }

  stream_on_ = true;
  return 0;
}

int V4L2CameraDevice::StreamOff() {
  base::AutoLock l(lock_);
  if (!device_fd_.is_valid()) {
    LOGF(ERROR) << "Device is not opened";
    return -ENODEV;
  }
  // Because UVC driver cannot allow STREAMOFF after REQBUF(0), adding a check
  // here to prevent it.
  if (!stream_on_) {
    return 0;
  }

  v4l2_buf_type capture_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (TEMP_FAILURE_RETRY(
          ioctl(device_fd_.get(), VIDIOC_STREAMOFF, &capture_type)) < 0) {
    LOGF(ERROR) << "STREAMOFF fails: " << strerror(errno);
    return -errno;
  }
  v4l2_requestbuffers req_buffers;
  memset(&req_buffers, 0, sizeof(req_buffers));
  req_buffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req_buffers.memory = V4L2_MEMORY_MMAP;
  req_buffers.count = 0;
  if (TEMP_FAILURE_RETRY(
          ioctl(device_fd_.get(), VIDIOC_REQBUFS, &req_buffers)) < 0) {
    LOGF(ERROR) << "REQBUFS fails: " << strerror(errno);
    return -errno;
  }
  buffers_at_client_.clear();
  stream_on_ = false;
  return 0;
}

int V4L2CameraDevice::GetNextFrameBuffer(uint32_t* buffer_id,
                                         uint32_t* data_size) {
  base::AutoLock l(lock_);
  if (!device_fd_.is_valid()) {
    LOGF(ERROR) << "Device is not opened";
    return -ENODEV;
  }
  if (!stream_on_) {
    LOGF(ERROR) << "Streaming is not started";
    return -EIO;
  }

  v4l2_buffer buffer;
  memset(&buffer, 0, sizeof(buffer));
  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_MMAP;
  if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_DQBUF, &buffer)) < 0) {
    LOGF(ERROR) << "DQBUF fails: " << strerror(errno);
    return -errno;
  }
  VLOGF(1) << "DQBUF returns index " << buffer.index << " length "
           << buffer.length;

  if (buffer.index >= buffers_at_client_.size() ||
      buffers_at_client_[buffer.index]) {
    LOGF(ERROR) << "Invalid buffer id " << buffer.index;
    return -EINVAL;
  }
  *buffer_id = buffer.index;
  *data_size = buffer.bytesused;
  buffers_at_client_[buffer.index] = true;
  return 0;
}

int V4L2CameraDevice::ReuseFrameBuffer(uint32_t buffer_id) {
  base::AutoLock l(lock_);
  if (!device_fd_.is_valid()) {
    LOGF(ERROR) << "Device is not opened";
    return -ENODEV;
  }
  if (!stream_on_) {
    LOGF(ERROR) << "Streaming is not started";
    return -EIO;
  }

  VLOGF(1) << "Reuse buffer id: " << buffer_id;
  if (buffer_id >= buffers_at_client_.size() ||
      !buffers_at_client_[buffer_id]) {
    LOGF(ERROR) << "Invalid buffer id: " << buffer_id;
    return -EINVAL;
  }
  v4l2_buffer buffer;
  memset(&buffer, 0, sizeof(buffer));
  buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buffer.memory = V4L2_MEMORY_MMAP;
  buffer.index = buffer_id;
  if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_QBUF, &buffer)) < 0) {
    LOGF(ERROR) << "QBUF fails: " << strerror(errno);
    return -errno;
  }
  buffers_at_client_[buffer.index] = false;
  return 0;
}

const SupportedFormats V4L2CameraDevice::GetDeviceSupportedFormats(
    const std::string& device_path) {
  VLOGF(1) << "Query supported formats for " << device_path;
  SupportedFormats formats;
  if (device_path.compare(0, strlen(kAllowedCameraPrefix),
                          kAllowedCameraPrefix) &&
      device_path.compare(0, strlen(kAllowedVideoPrefix),
                          kAllowedVideoPrefix)) {
    LOGF(ERROR) << "Invalid device path " << device_path;
    return formats;
  }
  // If device path is /dev/video*, it means the device is an external camera.
  // We have to glob all video devices again since the device number may be
  // changed after suspend/resume.
  std::string correct_device_path = device_path;
  if (!device_path.compare(0, strlen(kAllowedVideoPrefix),
                           kAllowedVideoPrefix)) {
    std::pair<std::string, DeviceInfo> external_camera = FindExternalCamera();
    if (external_camera.first != "") {
      correct_device_path = external_camera.second.device_path;
    }
  }

  base::ScopedFD fd(RetryDeviceOpen(correct_device_path, O_RDONLY));
  if (!fd.is_valid()) {
    LOGF(ERROR) << "Failed to open " << device_path << " : " << strerror(errno);
    return formats;
  }

  v4l2_fmtdesc v4l2_format = {};
  v4l2_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (;
       TEMP_FAILURE_RETRY(ioctl(fd.get(), VIDIOC_ENUM_FMT, &v4l2_format)) == 0;
       ++v4l2_format.index) {
    SupportedFormat supported_format;
    supported_format.fourcc = v4l2_format.pixelformat;

    v4l2_frmsizeenum frame_size = {};
    frame_size.pixel_format = v4l2_format.pixelformat;
    for (; HANDLE_EINTR(ioctl(fd.get(), VIDIOC_ENUM_FRAMESIZES, &frame_size)) ==
           0;
         ++frame_size.index) {
      if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        supported_format.width = frame_size.discrete.width;
        supported_format.height = frame_size.discrete.height;
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                 frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        // TODO(henryhsu): see http://crbug.com/249953, support these devices.
        LOGF(ERROR) << "Stepwise and continuous frame size are unsupported";
        return formats;
      }

      supported_format.frame_rates = GetFrameRateList(
          fd.get(), v4l2_format.pixelformat, frame_size.discrete.width,
          frame_size.discrete.height);
      formats.push_back(supported_format);
    }
  }
  return formats;
}

const DeviceInfos V4L2CameraDevice::GetCameraDeviceInfos() {
  // /dev/camera-internal* symbolic links should have been created and pointed
  // to the internal cameras according to Vid and Pid.
  std::unordered_map<std::string, DeviceInfo> camera_devices =
      GetCameraDevicesByPattern(std::string(kAllowedCameraPrefix) + "*");
  internal_devices_ = camera_devices;

  CameraCharacteristics characteristics;
  bool external_camera_support = characteristics.IsExternalCameraSupported();
  if (external_camera_support) {
    std::pair<std::string, DeviceInfo> external_camera = FindExternalCamera();
    if (external_camera.first != "") {
      VLOGF(1) << "Add external camera " << external_camera.first
               << ", path: " << external_camera.second.device_path;
      camera_devices.insert(external_camera);
    }
  }

  DeviceInfos device_infos =
      characteristics.GetCharacteristicsFromFile(camera_devices);

  if (device_infos.empty()) {
    // Symbolic link /dev/camera-internal* is generated from the udev rules
    // 50-camera.rules in chromeos-bsp-{BOARD}-private. The rules file may not
    // exist, and it will cause this error. (b/29425883)
    LOGF(ERROR) << "Cannot find any camera devices with "
                << kAllowedCameraPrefix << "*";
    LOGF(ERROR) << "List available cameras as follows: ";
    std::unordered_map<std::string, DeviceInfo> video_devices =
        GetCameraDevicesByPattern(std::string(kAllowedVideoPrefix) + "*");
    for (const auto& device : video_devices) {
      size_t pos = device.first.find(":");
      if (pos != std::string::npos) {
        LOGF(ERROR) << "Device path: " << device.second.device_path
                    << " vid: " << device.first.substr(0, pos)
                    << " pid: " << device.first.substr(pos + 1);
      } else {
        LOGF(ERROR) << "Invalid device: " << device.first;
      }
    }
    return DeviceInfos();
  } else {
    VLOGF(1) << "Number of cameras: " << device_infos.size();
  }
  return device_infos;
}

std::vector<float> V4L2CameraDevice::GetFrameRateList(int fd,
                                                      uint32_t fourcc,
                                                      uint32_t width,
                                                      uint32_t height) {
  std::vector<float> frame_rates;

  v4l2_frmivalenum frame_interval = {};
  frame_interval.pixel_format = fourcc;
  frame_interval.width = width;
  frame_interval.height = height;
  for (; TEMP_FAILURE_RETRY(
             ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval)) == 0;
       ++frame_interval.index) {
    if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
      if (frame_interval.discrete.numerator != 0) {
        frame_rates.push_back(
            frame_interval.discrete.denominator /
            static_cast<float>(frame_interval.discrete.numerator));
      }
    } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS ||
               frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
      // TODO(henryhsu): see http://crbug.com/249953, support these devices.
      LOGF(ERROR) << "Stepwise and continuous frame interval are unsupported";
      return frame_rates;
    }
  }
  // Some devices, e.g. Kinect, do not enumerate any frame rates, see
  // http://crbug.com/412284. Set their frame_rate to zero.
  if (frame_rates.empty()) {
    frame_rates.push_back(0);
  }
  return frame_rates;
}

const std::unordered_map<std::string, DeviceInfo>
V4L2CameraDevice::GetCameraDevicesByPattern(std::string pattern) {
  const base::FilePath path(pattern);
  base::FileEnumerator enumerator(path.DirName(), false,
                                  base::FileEnumerator::FILES,
                                  path.BaseName().value());
  std::unordered_map<std::string, DeviceInfo> devices;
  std::string device_path, device_name;

  while (!enumerator.Next().empty()) {
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    const std::string name = info.GetName().value();
    const base::FilePath target_path = path.DirName().Append(name);

    base::FilePath target;
    if (base::ReadSymbolicLink(target_path, &target)) {
      device_path = "/dev/" + target.value();
      device_name = target.value();
    } else {
      device_path = target_path.value();
      device_name = name;
    }

    const base::ScopedFD fd(
        TEMP_FAILURE_RETRY(open(device_path.c_str(), O_RDONLY | O_NOFOLLOW)));
    if (!fd.is_valid()) {
      VLOGF(1) << "Couldn't open " << device_name;
      continue;
    }

    v4l2_capability cap;
    DeviceInfo device_info = CameraCharacteristics::GetDefaultDeviceInfo();
    if ((TEMP_FAILURE_RETRY(ioctl(fd.get(), VIDIOC_QUERYCAP, &cap)) == 0) &&
        ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
         !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT))) {
      const std::string vidPath =
          base::StringPrintf(kVidPathTemplate, device_name.c_str());
      const std::string pidPath =
          base::StringPrintf(kPidPathTemplate, device_name.c_str());

      std::string usb_vid, usb_pid;
      if (!ReadIdFile(vidPath, &usb_vid)) {
        VLOGF(1) << "Couldn't read VID of " << device_name;
        continue;
      }
      if (!ReadIdFile(pidPath, &usb_pid)) {
        VLOGF(1) << "Couldn't read PID of " << device_name;
        continue;
      }
      VLOGF(1) << "Device path: " << target_path.value() << " vid: " << usb_vid
               << " pid: " << usb_pid;
      device_info.usb_vid = usb_vid;
      device_info.usb_pid = usb_pid;
      device_info.device_path = target_path.value();

      // Get power line frequency info
      device_info.power_line_frequency = GetPowerLineFrequency(fd.get());

      devices.insert(std::make_pair(usb_vid + ":" + usb_pid, device_info));
    }
  }
  if (devices.empty()) {
    VLOGF(1) << "Cannot find any camera devices with pattern " << pattern;
  }
  return devices;
}

int V4L2CameraDevice::RetryDeviceOpen(const std::string& device_path,
                                      int flags) {
  const int64_t kDeviceOpenTimeOutInMilliseconds = 2000;
  const int64_t kSleepTimeInMilliseconds = 100;
  int fd;
  base::ElapsedTimer timer;
  int64_t elapsed_time = timer.Elapsed().InMillisecondsRoundedUp();
  while (elapsed_time < kDeviceOpenTimeOutInMilliseconds) {
    fd = TEMP_FAILURE_RETRY(open(device_path.c_str(), flags));
    if (fd != -1) {
      // Make sure ioctl is ok. Once ioctl failed, we have to re-open the
      // device.
      struct v4l2_fmtdesc v4l2_format = {};
      v4l2_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      int ret = TEMP_FAILURE_RETRY(ioctl(fd, VIDIOC_ENUM_FMT, &v4l2_format));
      if (ret == -1) {
        close(fd);
        if (errno != EPERM) {
          break;
        } else {
          VLOGF(1) << "Camera ioctl is not ready";
        }
      } else {
        // Only return fd when ioctl is ready.
        if (elapsed_time >= kSleepTimeInMilliseconds) {
          LOGF(INFO) << "Opened the camera device after waiting for "
                     << elapsed_time << " ms";
        }
        return fd;
      }
    } else if (errno != ENOENT) {
      break;
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kSleepTimeInMilliseconds));
    elapsed_time = timer.Elapsed().InMillisecondsRoundedUp();
  }
  LOGF(ERROR) << "Failed to open " << device_path << " : " << strerror(errno);
  return -1;
}

std::pair<std::string, DeviceInfo> V4L2CameraDevice::FindExternalCamera() {
  std::unordered_map<std::string, DeviceInfo> video_devices =
      GetCameraDevicesByPattern(std::string(kAllowedVideoPrefix) + "*");

  if (internal_devices_.size() == 0) {
    internal_devices_ =
        GetCameraDevicesByPattern(std::string(kAllowedCameraPrefix) + "*");
  }

  for (const auto& device : internal_devices_) {
    base::FilePath target;
    std::string device_path;
    if (base::ReadSymbolicLink(base::FilePath(device.second.device_path),
                               &target)) {
      device_path = "/dev/" + target.value();
    } else {
      LOGF(ERROR) << device.second.device_path << " should be a symbolic link";
    }
    video_devices.erase(device.first);
  }

  if (video_devices.size() > 1) {
    LOGF(ERROR) << "Only allow one external camera";
    return std::make_pair("", DeviceInfo());
  }

  for (const auto& device : video_devices) {
    VLOGF(1) << "Find external camera " << device.first
             << ", path: " << device.second.device_path;
    return device;
  }
  return std::make_pair("", DeviceInfo());
}

PowerLineFrequency V4L2CameraDevice::GetPowerLineFrequency(int fd) {
  struct v4l2_queryctrl query = {};
  query.id = V4L2_CID_POWER_LINE_FREQUENCY;
  if (TEMP_FAILURE_RETRY(ioctl(fd, VIDIOC_QUERYCTRL, &query)) < 0) {
    LOGF(ERROR) << "Power line frequency should support auto or 50/60Hz";
    return PowerLineFrequency::FREQ_ERROR;
  }

  PowerLineFrequency frequency = GetPowerLineFrequencyForLocation();
  if (frequency == PowerLineFrequency::FREQ_DEFAULT) {
    switch (query.default_value) {
      case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
        frequency = PowerLineFrequency::FREQ_50HZ;
        break;
      case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
        frequency = PowerLineFrequency::FREQ_60HZ;
        break;
      case V4L2_CID_POWER_LINE_FREQUENCY_AUTO:
        frequency = PowerLineFrequency::FREQ_AUTO;
        break;
      default:
        break;
    }
  }

  // Prefer auto setting if camera module supports auto mode.
  if (query.maximum == V4L2_CID_POWER_LINE_FREQUENCY_AUTO) {
    frequency = PowerLineFrequency::FREQ_AUTO;
  } else if (query.minimum >= V4L2_CID_POWER_LINE_FREQUENCY_60HZ) {
    LOGF(ERROR) << "Camera module should at least support 50/60Hz";
    return PowerLineFrequency::FREQ_ERROR;
  }
  return frequency;
}

int V4L2CameraDevice::SetPowerLineFrequency(PowerLineFrequency setting) {
  int v4l2_freq_setting = V4L2_CID_POWER_LINE_FREQUENCY_DISABLED;
  switch (setting) {
    case PowerLineFrequency::FREQ_50HZ:
      v4l2_freq_setting = V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
      break;
    case PowerLineFrequency::FREQ_60HZ:
      v4l2_freq_setting = V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
      break;
    case PowerLineFrequency::FREQ_AUTO:
      v4l2_freq_setting = V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
      break;
    default:
      LOGF(ERROR) << "Invalid setting for power line frequency: " << setting;
      return -EINVAL;
  }

  struct v4l2_control control = {};
  control.id = V4L2_CID_POWER_LINE_FREQUENCY;
  control.value = v4l2_freq_setting;
  if (TEMP_FAILURE_RETRY(ioctl(device_fd_.get(), VIDIOC_S_CTRL, &control)) <
      0) {
    LOGF(ERROR) << "Error setting power line frequency to "
                << v4l2_freq_setting;
    return -EINVAL;
  }
  VLOGF(1) << "Set power line frequency(" << setting << ") successfully";
  return 0;
}

}  // namespace arc
