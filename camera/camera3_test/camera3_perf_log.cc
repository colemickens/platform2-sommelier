// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_perf_log.h"

#include <inttypes.h>

#include <string>
#include <utility>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>

#include "cros-camera/common.h"

namespace camera3_test {

// static
Camera3PerfLog* Camera3PerfLog::GetInstance() {
  static Camera3PerfLog perf;
  return &perf;
}

Camera3PerfLog::~Camera3PerfLog() {
  VLOGF_ENTER();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("output_log")) {
    VLOGF(1) << "Outputing to log file: "
             << base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                    "output_log");
    base::FilePath file_path(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            "output_log"));
    if (base::WriteFile(file_path, NULL, 0) < 0) {
      LOGF(ERROR) << "Error writing to file " << file_path.value();
      return;
    }
    std::map<Key, std::string> KeyNameMap = {
        {DEVICE_OPENED, "device_open"},
        {PREVIEW_STARTED, "preview_start"},
        {STILL_IMAGE_CAPTURED, "still_image_capture"},
    };
    for (const auto& it : perf_log_map_) {
      int cam_id = it.first;
      if (it.second.find(DEVICE_OPENING) == it.second.end()) {
        LOGF(ERROR) << "Failed to find device opening performance log";
        continue;
      }
      std::string s = base::StringPrintf("Camera: %s\n",
                                         GetCameraNameForId(cam_id).c_str());
      base::AppendToFile(file_path, s.c_str(), s.length());
      for (const auto& jt : it.second) {
        Key key = jt.first;
        if (KeyNameMap.find(key) == KeyNameMap.end()) {
          continue;
        }
        base::TimeTicks end_ticks = jt.second;
        base::TimeTicks start_ticks = it.second.at(DEVICE_OPENING);
        std::string s =
            base::StringPrintf("%s: %" PRId64 " us\n", KeyNameMap[key].c_str(),
                               (end_ticks - start_ticks).InMicroseconds());
        base::AppendToFile(file_path, s.c_str(), s.length());
      }
      if (still_capture_perf_log_map_.find(cam_id) !=
              still_capture_perf_log_map_.end() &&
          still_capture_perf_log_map_[cam_id].size() > 1) {
        std::string s =
            base::StringPrintf("shot_to_shot: %" PRId64 " us\n",
                               (still_capture_perf_log_map_[cam_id][1] -
                                still_capture_perf_log_map_[cam_id][0])
                                   .InMicroseconds());
        base::AppendToFile(file_path, s.c_str(), s.length());
      }
    }
  }
}

void Camera3PerfLog::SetCameraNameMap(
    const std::map<int, std::string>& camera_name_map) {
  camera_name_map_ = camera_name_map;
}

std::string Camera3PerfLog::GetCameraNameForId(int id) {
  auto it = camera_name_map_.find(id);
  return it != camera_name_map_.end() ? it->second : std::to_string(id);
}

bool Camera3PerfLog::Update(int cam_id, Key key, base::TimeTicks time) {
  if (key >= END_OF_KEY) {
    return false;
  }
  VLOGF(1) << "Updating key " << key << " of camera " << cam_id << " at "
           << time << " us";
  if (perf_log_map_[cam_id].find(key) == perf_log_map_[cam_id].end()) {
    perf_log_map_[cam_id][key] = time;
  } else if (key != STILL_IMAGE_CAPTURED) {
    LOGF(ERROR) << "The key " << key << " is being updated twice";
    return false;
  }
  if (key == STILL_IMAGE_CAPTURED) {
    still_capture_perf_log_map_[cam_id].push_back(time);
  }
  return true;
}

}  // namespace camera3_test
