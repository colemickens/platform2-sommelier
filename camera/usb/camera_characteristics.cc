/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb/camera_characteristics.h"

#include <vector>

#include <base/files/file_util.h>

#include "arc/common.h"

namespace arc {

// /etc/camera/camera_characteristics.conf contains camera information which
// driver cannot provide.
static const char kCameraCharacteristicsConfigFile[] =
    "/etc/camera/camera_characteristics.conf";
static const char kLensFacing[] = "lens_facing";
static const char kSensorOrientation[] = "sensor_orientation";
static const char kUsbVidPid[] = "usb_vid_pid";
static const char kFramesToSkipAfterStreamon[] =
    "frames_to_skip_after_streamon";
static const char kHorizontalViewAngle_16_9[] = "horizontal_view_angle_16_9";
static const char kHorizontalViewAngle_4_3[] = "horizontal_view_angle_4_3";
static const char kLensInfoAvailableFocalLengths[] =
    "lens_info_available_focal_lengths";
static const char kLensInfoMinimumFocusDistance[] =
    "lens_info_minimum_focus_distance";
static const char kLensInfoOptimalFocusDistance[] =
    "lens_info_optimal_focus_distance";
static const char kVerticalViewAngle_16_9[] = "vertical_view_angle_16_9";
static const char kVerticalViewAngle_4_3[] = "vertical_view_angle_4_3";

/* HAL v3 parameters */
static const char kLensInfoAvailableApertures[] =
    "lens_info_available_apertures";
static const char kSensorInfoPhysicalSize[] = "sensor_info_physical_size";
static const char kSensorInfoPixelArraySize[] = "sensor_info_pixel_array_size";

static const struct DeviceInfo kDefaultCharacteristics = {
    "",     // device_path
    "",     // usb_vid
    "",     // usb_pid
    0,      // lens_facing
    0,      // sensor_orientation
    0,      // frames_to_skip_after_streamon
    66.5,   // horizontal_view_angle_16_9
    0.0,    // horizontal_view_angle_4_3
    {1.6},  // lens_info_available_focal_lengths
    0.3,    // lens_info_minimum_focus_distance
    0.5,    // lens_info_optimal_focus_distance
    42.5,   // vertical_view_angle_16_9
    0.0     // vertical_view_angle_4_3
};

CameraCharacteristics::CameraCharacteristics() {}

CameraCharacteristics::~CameraCharacteristics() {}

const DeviceInfos CameraCharacteristics::GetCharacteristicsFromFile(
    const std::unordered_map<std::string, std::string>& devices) {
  const base::FilePath path(kCameraCharacteristicsConfigFile);
  FILE* file = base::OpenFile(path, "r");
  if (!file) {
    LOGF(ERROR) << "Can't open file " << kCameraCharacteristicsConfigFile
                << ". Use default characteristics instead";
    for (const auto& device : devices) {
      device_infos_.push_back(kDefaultCharacteristics);
      size_t pos = device.first.find(":");
      if (pos != std::string::npos) {
        device_infos_.back().device_path = device.second;
        device_infos_.back().usb_vid = device.first.substr(0, pos - 1);
        device_infos_.back().usb_pid = device.first.substr(pos + 1);
      } else {
        LOGF(ERROR) << "Invalid device: " << device.first;
      }
    }
    return device_infos_;
  }

  char buffer[256], key[256], value[256];
  uint32_t camera_id;
  uint32_t module_id = -1;
  std::string vid, pid;
  while (fgets(buffer, sizeof(buffer), file)) {
    // Skip comments and empty lines.
    if (buffer[0] == '#' || buffer[0] == '\n') {
      continue;
    }

    if (sscanf(buffer, "%[^=]=%s", key, value) != 2) {
      LOGF(ERROR) << "Illegal format: " << buffer;
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
    if (camera_id > device_infos_.size()) {
      // Camera id should be ascending by one.
      LOGF(ERROR) << "Invalid camera id: " << camera_id;
      continue;
    } else if (camera_id == device_infos_.size()) {
      device_infos_.push_back(kDefaultCharacteristics);
    }

    uint32_t tmp_module_id;
    if (sscanf(sub_keys[1], "module%u", &tmp_module_id) != 1) {
      AddPerCameraCharacteristic(camera_id, sub_keys[1], value);
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
          device_infos_[camera_id].usb_vid = vid;
          device_infos_[camera_id].usb_pid = pid;
          device_infos_[camera_id].device_path = device->second;
        }

        VLOGF(1) << "Camera" << camera_id << " " << kUsbVidPid << ": " << value;
      } else if (!vid.empty() && !pid.empty()) {
        // Some characteristics are module-specific, so only matched ones are
        // selected.
        if (device_infos_[camera_id].usb_vid != vid ||
            device_infos_[camera_id].usb_pid != pid) {
          VLOGF(1) << "Mismatched module: "
                   << "vid: " << vid << " pid: " << pid;
          continue;
        }
        AddPerModuleCharacteristic(camera_id, sub_keys[2], value);
      } else {
        // Characteristic usb_vid_pid should come before other module-specific
        // characteristics.
        LOGF(ERROR) << "Illegal format. usb_vid_pid should come before: "
                    << buffer;
      }
    }
  }
  base::CloseFile(file);
  for (size_t id = 0; id < device_infos_.size(); ++id) {
    if (device_infos_[id].device_path.empty()) {
      LOGF(ERROR) << "No matching module for camera" << id;
      return DeviceInfos();
    }
  }
  return device_infos_;
}

void CameraCharacteristics::AddPerCameraCharacteristic(
    uint32_t camera_id,
    const char* characteristic,
    const char* value) {
  if (strcmp(characteristic, kLensFacing) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    device_infos_[camera_id].lens_facing = strtol(value, NULL, 10);
  } else if (strcmp(characteristic, kSensorOrientation) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    device_infos_[camera_id].sensor_orientation = strtol(value, NULL, 10);
  } else {
    LOGF(ERROR) << "Unknown characteristic: " << characteristic
                << " value: " << value;
  }
}

void CameraCharacteristics::AddPerModuleCharacteristic(
    uint32_t camera_id,
    const char* characteristic,
    const char* value) {
  if (strcmp(characteristic, kFramesToSkipAfterStreamon) == 0) {
    VLOGF(1) << characteristic << ": " << value;
    device_infos_[camera_id].frames_to_skip_after_streamon =
        strtol(value, NULL, 10);
  } else if (strcmp(characteristic, kHorizontalViewAngle_16_9) == 0) {
    AddFloatValue(value, kHorizontalViewAngle_16_9,
                  &device_infos_[camera_id].horizontal_view_angle_16_9);
  } else if (strcmp(characteristic, kHorizontalViewAngle_4_3) == 0) {
    AddFloatValue(value, kHorizontalViewAngle_4_3,
                  &device_infos_[camera_id].horizontal_view_angle_4_3);
  } else if (strcmp(characteristic, kLensInfoAvailableFocalLengths) == 0) {
    device_infos_[camera_id].lens_info_available_focal_lengths.clear();
    char tmp_value[256];
    snprintf(tmp_value, sizeof(tmp_value), "%s", value);
    char* save_ptr;
    char* focal_length = strtok_r(tmp_value, ",", &save_ptr);
    while (focal_length) {
      float tmp_focal_length = strtof(focal_length, NULL);
      if (tmp_focal_length != 0.0) {
        VLOGF(1) << characteristic << ": " << tmp_focal_length;
        device_infos_[camera_id].lens_info_available_focal_lengths.push_back(
            tmp_focal_length);
      } else {
        LOGF(ERROR) << "Invalid " << characteristic << ": " << value;
        device_infos_[camera_id].lens_info_available_focal_lengths.clear();
        device_infos_[camera_id].lens_info_available_focal_lengths.push_back(
            kDefaultCharacteristics.lens_info_available_focal_lengths[0]);
        break;
      }
      focal_length = strtok_r(NULL, ",", &save_ptr);
    }
  } else if (strcmp(characteristic, kLensInfoMinimumFocusDistance) == 0) {
    AddFloatValue(value, kLensInfoMinimumFocusDistance,
                  &device_infos_[camera_id].lens_info_minimum_focus_distance);
  } else if (strcmp(characteristic, kLensInfoOptimalFocusDistance) == 0) {
    AddFloatValue(value, kLensInfoOptimalFocusDistance,
                  &device_infos_[camera_id].lens_info_optimal_focus_distance);
  } else if (strcmp(characteristic, kVerticalViewAngle_16_9) == 0) {
    AddFloatValue(value, kVerticalViewAngle_16_9,
                  &device_infos_[camera_id].vertical_view_angle_16_9);
  } else if (strcmp(characteristic, kVerticalViewAngle_4_3) == 0) {
    AddFloatValue(value, kVerticalViewAngle_4_3,
                  &device_infos_[camera_id].vertical_view_angle_4_3);
  } else if (strcmp(characteristic, kLensInfoAvailableApertures) == 0) {
    /* Do nothing. This is for hal v3 */
  } else if (strcmp(characteristic, kSensorInfoPhysicalSize) == 0) {
    /* Do nothing. This is for hal v3 */
  } else if (strcmp(characteristic, kSensorInfoPixelArraySize) == 0) {
    /* Do nothing. This is for hal v3 */
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

}  // namespace arc
