// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_PERF_LOG_H_
#define CAMERA3_TEST_CAMERA3_PERF_LOG_H_

#include <map>
#include <vector>

#include <base/time/time.h>

namespace camera3_test {

class Camera3PerfLog final {
 public:
  enum Key {
    DEVICE_OPENING,
    DEVICE_OPENED,
    PREVIEW_STARTED,
    STILL_IMAGE_CAPTURED,
    END_OF_KEY
  };

  // Gets the singleton instance
  static Camera3PerfLog* GetInstance();

  // Update performance log
  bool Update(int cam_id, Key key, base::TimeTicks time);

 private:
  Camera3PerfLog() {}

  ~Camera3PerfLog();

  // Record performance logs in a map with camera id and Camera3PerfLog::Key as
  // the keys
  std::map<int, std::map<Key, base::TimeTicks>> perf_log_map_;

  // Record taking still picture performance logs in a map with camera id as
  // the key
  std::map<int, std::vector<base::TimeTicks>> still_capture_perf_log_map_;

  DISALLOW_COPY_AND_ASSIGN(Camera3PerfLog);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_PERF_LOG_H_
