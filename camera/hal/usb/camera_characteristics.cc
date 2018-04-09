/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal/usb/camera_characteristics.h"

#include <ios>
#include <sstream>
#include <vector>

#include <base/files/file_util.h>
#include <base/strings/string_split.h>

#include "cros-camera/common.h"
#include "cros-camera/timezone.h"

namespace cros {

// /etc/camera/camera_characteristics.conf contains camera information which
// driver cannot provide.
static const char kCameraCharacteristicsConfigFile[] =
    "/etc/camera/camera_characteristics.conf";

/* Common parameters */
static const char kLensFacing[] = "lens_facing";
static const char kSensorOrientation[] = "sensor_orientation";
static const char kUsbVidPid[] = "usb_vid_pid";
static const char kLensInfoAvailableFocalLengths[] =
    "lens_info_available_focal_lengths";
static const char kLensInfoMinimumFocusDistance[] =
    "lens_info_minimum_focus_distance";
static const char kLensInfoOptimalFocusDistance[] =
    "lens_info_optimal_focus_distance";

/* HAL v1 parameters */
static const char kHorizontalViewAngle_16_9[] = "horizontal_view_angle_16_9";
static const char kHorizontalViewAngle_4_3[] = "horizontal_view_angle_4_3";
static const char kVerticalViewAngle_16_9[] = "vertical_view_angle_16_9";
static const char kVerticalViewAngle_4_3[] = "vertical_view_angle_4_3";

/* HAL v3 parameters */
static const char kLensInfoAvailableApertures[] =
    "lens_info_available_apertures";
static const char kSensorInfoPhysicalSize[] = "sensor_info_physical_size";
static const char kSensorInfoPixelArraySize[] = "sensor_info_pixel_array_size";

/* Special parameters */
static const char kFramesToSkipAfterStreamon[] =
    "frames_to_skip_after_streamon";
static const char kResolution1280x960Unsupported[] =
    "resolution_1280x960_unsupported";
static const char kResolution1600x1200Unsupported[] =
    "resolution_1600x1200_unsupported";
static const char kConstantFramerateUnsupported[] =
    "constant_framerate_unsupported";

/* Global parameters */
static const char kAllowExternalCamera[] = "allow_external_camera";

static const struct DeviceInfo kDefaultCharacteristics = {
    "",                                // device_path
    "",                                // usb_vid
    "",                                // usb_pid
    0,                                 // frames_to_skip_after_streamon
    PowerLineFrequency::FREQ_DEFAULT,  // power_line_frequency
    0,                                 // lens_facing
    0,                                 // sensor_orientation
    66.5,                              // horizontal_view_angle_16_9
    0.0,                               // horizontal_view_angle_4_3
    {1.6},                             // lens_info_available_focal_lengths
    0.3,                               // lens_info_minimum_focus_distance
    0.5,                               // lens_info_optimal_focus_distance
    42.5,                              // vertical_view_angle_16_9
    0.0,                               // vertical_view_angle_4_3
    false,                             // resolution_1280x960_unsupported
    false,                             // resolution_1600x1200_unsupported
    false,                             // constant_framerate_unsupported
    0,                                 // sensor_info_pixel_array_size_width
    0,                                 // sensor_info_pixel_array_size_height
    {2.0},                             // lens_info_available_apertures
    0,                                 // sensor_info_physical_size_width
    0                                  // sensor_info_physical_size_height
};

CameraCharacteristics::CameraCharacteristics() {}

CameraCharacteristics::~CameraCharacteristics() {}

// static
DeviceInfo CameraCharacteristics::GetDefaultDeviceInfo() {
  return kDefaultCharacteristics;
}

// static
bool CameraCharacteristics::ConfigFileExists() {
  return base::PathExists(base::FilePath(kCameraCharacteristicsConfigFile));
}

const DeviceInfos CameraCharacteristics::GetCharacteristicsFromFile(
    const std::unordered_map<std::string, DeviceInfo>& devices) {
  const base::FilePath path(kCameraCharacteristicsConfigFile);
  FILE* file = base::OpenFile(path, "r");
  if (!file) {
    LOGF(INFO) << "Can't open file " << kCameraCharacteristicsConfigFile;
    return DeviceInfos();
  }

  DeviceInfos tmp_device_infos;
  char buffer[256], key[256], value[256];
  uint32_t camera_id;
  uint32_t module_id = -1;
  std::string vid, pid;
  bool allow_external_camera = false;
  while (fgets(buffer, sizeof(buffer), file)) {
    // Skip comments and empty lines.
    if (buffer[0] == '#' || buffer[0] == '\n') {
      continue;
    }

    if (sscanf(buffer, "%[^=]=%s", key, value) != 2) {
      LOGF(ERROR) << "Illegal format: " << buffer;
      continue;
    }

    // global config
    if (strcmp(key, kAllowExternalCamera) == 0) {
      VLOGF(1) << "Allow external camera";
      std::istringstream is(value);
      is >> std::boolalpha >> allow_external_camera;
      continue;
    }

    std::vector<char*> sub_keys;
    char* save_ptr;
    char* sub_key = strtok_r(key, ".", &save_ptr);
    while (sub_key) {
      sub_keys.push_back(sub_key);
      sub_key = strtok_r(NULL, ".", &save_ptr);
    }

    if (sscanf(sub_keys[0], "camera%u", &camera_id) != 1) {
      LOGF(ERROR) << "Illegal format: " << sub_keys[0];
      continue;
    }
    if (camera_id > tmp_device_infos.size()) {
      // Camera id should be ascending by one.
      LOGF(ERROR) << "Invalid camera id: " << camera_id;
      continue;
    } else if (camera_id == tmp_device_infos.size()) {
      tmp_device_infos.push_back(kDefaultCharacteristics);
    }

    uint32_t tmp_module_id;
    // Convert value to lower case.
    for (char* p = value; *p; ++p)
      *p = tolower(*p);

    if (sscanf(sub_keys[1], "module%u", &tmp_module_id) != 1) {
      AddPerCameraCharacteristic(camera_id, sub_keys[1], value,
                                 &tmp_device_infos);
    } else {
      if (tmp_module_id != module_id) {
        vid.clear();
        pid.clear();
        module_id = tmp_module_id;
      }
      if (strcmp(sub_keys[2], kUsbVidPid) == 0) {
        char tmp_vid[256], tmp_pid[256];
        if (sscanf(value, "%[0-9a-z]:%[0-9a-z]", tmp_vid, tmp_pid) != 2) {
          LOGF(ERROR) << "Invalid format: " << sub_keys[2];
          continue;
        }
        vid = tmp_vid;
        pid = tmp_pid;
        const auto& device = devices.find(value);
        if (device != devices.end()) {
          tmp_device_infos[camera_id].usb_vid = device->second.usb_vid;
          tmp_device_infos[camera_id].usb_pid = device->second.usb_pid;
          tmp_device_infos[camera_id].device_path = device->second.device_path;
          tmp_device_infos[camera_id].power_line_frequency =
              device->second.power_line_frequency;
        }

        VLOGF(1) << "Camera" << camera_id << " " << kUsbVidPid << ": " << value;
      } else if (!vid.empty() && !pid.empty()) {
        // Some characteristics are module-specific, so only matched ones are
        // selected.
        if (tmp_device_infos[camera_id].usb_vid != vid ||
            tmp_device_infos[camera_id].usb_pid != pid) {
          VLOGF(1) << "Mismatched module: "
                   << "vid: " << vid << " pid: " << pid;
          continue;
        }
        AddPerModuleCharacteristic(camera_id, sub_keys[2], value,
                                   &tmp_device_infos);
      } else {
        // Characteristic usb_vid_pid should come before other module-specific
        // characteristics.
        LOGF(ERROR) << "Illegal format. usb_vid_pid should come before: "
                    << buffer;
      }
    }
  }
  base::CloseFile(file);

  DeviceInfos device_infos;
  // Some devices use the same camera_characteristics.conf and have different
  // number of cameras.
  for (size_t id = 0; id < tmp_device_infos.size(); ++id) {
    if (tmp_device_infos[id].device_path.empty()) {
      LOGF(INFO) << "No matching module for camera" << id;
    } else {
      for (const auto& device_info : device_infos) {
        if (device_info.usb_vid == tmp_device_infos[id].usb_vid &&
            device_info.usb_pid == tmp_device_infos[id].usb_pid) {
          LOGF(ERROR) << "Module " << device_info.usb_vid << ":"
                      << device_info.usb_pid
                      << " should not match multiple configs";
          return DeviceInfos();
        }
      }
      device_infos.push_back(tmp_device_infos[id]);
    }
  }

  // If device allows external cameras, add the camera into the end of
  // |device_infos|.
  if (allow_external_camera) {
    AddExternalCameras(devices, &device_infos);
  }

  // Check sensor array size to decide supported resolutions.
  for (size_t id = 0; id < device_infos.size(); ++id) {
    if (device_infos[id].sensor_info_pixel_array_size_width < 1280 ||
        device_infos[id].sensor_info_pixel_array_size_height < 960) {
      device_infos[id].resolution_1280x960_unsupported = true;
    }
    if (device_infos[id].sensor_info_pixel_array_size_width < 1600 ||
        device_infos[id].sensor_info_pixel_array_size_height < 1200) {
      device_infos[id].resolution_1600x1200_unsupported = true;
    }
  }
  return device_infos;
}

bool CameraCharacteristics::IsExternalCameraSupported() {
  const base::FilePath path(kCameraCharacteristicsConfigFile);
  std::string contents;
  bool ret = base::ReadFileToString(path, &contents);
  if (ret == false) {
    return false;
  }
  std::string pattern(kAllowExternalCamera);
  pattern += "=true";
  return contents.find(pattern) != std::string::npos;
}

void CameraCharacteristics::AddPerCameraCharacteristic(
    uint32_t camera_id,
    const char* characteristic,
    const char* value,
    DeviceInfos* device_infos) {
  VLOGF(1) << characteristic << ": " << value;
  if (strcmp(characteristic, kLensFacing) == 0) {
    (*device_infos)[camera_id].lens_facing = strtol(value, NULL, 10);
  } else if (strcmp(characteristic, kSensorOrientation) == 0) {
    (*device_infos)[camera_id].sensor_orientation = strtol(value, NULL, 10);
  } else {
    LOGF(ERROR) << "Unknown characteristic: " << characteristic
                << " value: " << value;
  }
}

void CameraCharacteristics::AddPerModuleCharacteristic(
    uint32_t camera_id,
    const char* characteristic,
    const char* value,
    DeviceInfos* device_infos) {
  if (strcmp(characteristic, kFramesToSkipAfterStreamon) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    (*device_infos)[camera_id].frames_to_skip_after_streamon =
        strtol(value, NULL, 10);
  } else if (strcmp(characteristic, kHorizontalViewAngle_16_9) == 0) {
    AddFloatValue(value, kHorizontalViewAngle_16_9,
                  &(*device_infos)[camera_id].horizontal_view_angle_16_9);
  } else if (strcmp(characteristic, kHorizontalViewAngle_4_3) == 0) {
    AddFloatValue(value, kHorizontalViewAngle_4_3,
                  &(*device_infos)[camera_id].horizontal_view_angle_4_3);
  } else if (strcmp(characteristic, kLensInfoAvailableFocalLengths) == 0) {
    (*device_infos)[camera_id].lens_info_available_focal_lengths.clear();
    char tmp_value[256];
    snprintf(tmp_value, sizeof(tmp_value), "%s", value);
    char* save_ptr;
    char* focal_length = strtok_r(tmp_value, ",", &save_ptr);
    while (focal_length) {
      float tmp_focal_length = strtof(focal_length, NULL);
      if (tmp_focal_length != 0.0) {
        VLOGF(1) << characteristic << ": " << tmp_focal_length;
        (*device_infos)[camera_id].lens_info_available_focal_lengths.push_back(
            tmp_focal_length);
      } else {
        LOGF(ERROR) << "Invalid " << characteristic << ": " << value;
        (*device_infos)[camera_id].lens_info_available_focal_lengths.clear();
        (*device_infos)[camera_id].lens_info_available_focal_lengths.push_back(
            kDefaultCharacteristics.lens_info_available_focal_lengths[0]);
        break;
      }
      focal_length = strtok_r(NULL, ",", &save_ptr);
    }
  } else if (strcmp(characteristic, kLensInfoMinimumFocusDistance) == 0) {
    AddFloatValue(value, kLensInfoMinimumFocusDistance,
                  &(*device_infos)[camera_id].lens_info_minimum_focus_distance);
  } else if (strcmp(characteristic, kLensInfoOptimalFocusDistance) == 0) {
    AddFloatValue(value, kLensInfoOptimalFocusDistance,
                  &(*device_infos)[camera_id].lens_info_optimal_focus_distance);
  } else if (strcmp(characteristic, kVerticalViewAngle_16_9) == 0) {
    AddFloatValue(value, kVerticalViewAngle_16_9,
                  &(*device_infos)[camera_id].vertical_view_angle_16_9);
  } else if (strcmp(characteristic, kVerticalViewAngle_4_3) == 0) {
    AddFloatValue(value, kVerticalViewAngle_4_3,
                  &(*device_infos)[camera_id].vertical_view_angle_4_3);
  } else if (strcmp(characteristic, kLensInfoAvailableApertures) == 0) {
    (*device_infos)[camera_id].lens_info_available_apertures.clear();
    char tmp_value[256];
    snprintf(tmp_value, sizeof(tmp_value), "%s", value);
    char* save_ptr;
    char* aperture = strtok_r(tmp_value, ",", &save_ptr);
    while (aperture) {
      float tmp_aperture = strtof(aperture, NULL);
      if (tmp_aperture > 0.0) {
        VLOGF(1) << characteristic << ": " << tmp_aperture;
        (*device_infos)[camera_id].lens_info_available_apertures.push_back(
            tmp_aperture);
      } else {
        LOGF(ERROR) << "Invalid " << characteristic << ": " << value;
        (*device_infos)[camera_id].lens_info_available_apertures.clear();
        (*device_infos)[camera_id].lens_info_available_apertures.push_back(
            kDefaultCharacteristics.lens_info_available_apertures[0]);
        break;
      }
      aperture = strtok_r(NULL, ",", &save_ptr);
    }
  } else if (strcmp(characteristic, kSensorInfoPhysicalSize) == 0) {
    float width, height;
    if (sscanf(value, "%fx%f", &width, &height) != 2) {
      LOG(ERROR) << __func__ << ": Illegal physical size format: " << value;
      return;
    }
    VLOG(1) << __func__ << ": " << characteristic << ": " << width << "x"
            << height;
    (*device_infos)[camera_id].sensor_info_physical_size_width = width;
    (*device_infos)[camera_id].sensor_info_physical_size_height = height;
  } else if (strcmp(characteristic, kSensorInfoPixelArraySize) == 0) {
    int width, height;
    if (sscanf(value, "%dx%d", &width, &height) != 2) {
      LOGF(ERROR) << "Illegal array size format: " << value;
      return;
    }
    VLOGF(1) << characteristic << ": " << width << "x" << height;
    (*device_infos)[camera_id].sensor_info_pixel_array_size_width = width;
    (*device_infos)[camera_id].sensor_info_pixel_array_size_height = height;
  } else if (strcmp(characteristic, kResolution1280x960Unsupported) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    std::istringstream is(value);
    is >> std::boolalpha >>
        (*device_infos)[camera_id].resolution_1280x960_unsupported;
  } else if (strcmp(characteristic, kResolution1600x1200Unsupported) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    std::istringstream is(value);
    is >> std::boolalpha >>
        (*device_infos)[camera_id].resolution_1600x1200_unsupported;
  } else if (strcmp(characteristic, kConstantFramerateUnsupported) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    std::istringstream is(value);
    is >> std::boolalpha >>
        (*device_infos)[camera_id].constant_framerate_unsupported;
  } else {
    LOGF(ERROR) << "Unknown characteristic: " << characteristic
                << " value: " << value;
  }
}

void CameraCharacteristics::AddFloatValue(const char* value,
                                          const char* characteristic_name,
                                          float* characteristic) {
  float tmp_value = strtof(value, NULL);
  if (tmp_value != 0.0) {
    VLOGF(1) << characteristic_name << ": " << value;
    *characteristic = tmp_value;
  } else {
    LOGF(ERROR) << "Invalid " << characteristic_name << ": " << value;
  }
}

void CameraCharacteristics::AddExternalCameras(
    const std::unordered_map<std::string, DeviceInfo>& devices,
    DeviceInfos* device_infos) {
  for (const auto& device : devices) {
    bool device_exist = false;
    for (const auto& info : *device_infos) {
      if (device.second.device_path == info.device_path) {
        device_exist = true;
        break;
      }
    }
    if (device_exist == false) {
      std::vector<std::string> usb_vid_pid = base::SplitString(
          device.first, ":", base::WhitespaceHandling::TRIM_WHITESPACE,
          base::SplitResult::SPLIT_WANT_ALL);
      DeviceInfo device_info = device.second;
      device_infos->push_back(device_info);
      VLOGF(1) << "Add external camera: " << device.first << ", "
               << device.second.device_path;
    }
  }
}

}  // namespace cros
