// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/strings/stringprintf.h>
#include <brillo/flag_helper.h>

namespace {

constexpr struct input_event kSync = {
    .type = EV_SYN, .code = SYN_REPORT, .value = 0};

constexpr int kBitsPerInt = sizeof(uint32_t) * 8;
constexpr int kMaxInputDev = 256;
const int kMaxBit = std::max(std::max(EV_MAX, KEY_MAX), SW_MAX);
const int kMaxInt = (kMaxBit - 1) / kBitsPerInt + 1;

bool TestBit(const uint32_t bitmask[], int bit) {
  return ((bitmask[bit / kBitsPerInt] >> (bit % kBitsPerInt)) & 1);
}

bool HasEventBit(int fd, int event_type, int bit) {
  uint32_t bitmask[kMaxInt];
  memset(bitmask, 0, sizeof(bitmask));
  if (ioctl(fd, EVIOCGBIT(event_type, sizeof(bitmask)), bitmask) < 0)
    return false;
  return TestBit(bitmask, bit);
}

void LogErrorExit(const std::string& message) {
  LOG(ERROR) << message;
  exit(1);
}

// CreateEvent return a correctly filled input_event. It aborts on
// invalid |code| or |value|.
// |code|: "tablet", "lid"
// |value|: 0, 1
struct input_event CreateEvent(const std::string& code, int32_t value) {
  struct input_event event;
  event.type = EV_SW;
  if (code == "tablet")
    event.code = SW_TABLET_MODE;
  else if (code == "lid")
    event.code = SW_LID;
  else
    LogErrorExit("--code=<tablet|lid>");
  if (value != 0 && value != 1)
    LogErrorExit("--value=<0|1>");
  event.value = value;
  return event;
}

// Find the event device which supports said |type| and |code| and
// return opened file descriptor to it.
base::ScopedFD OpenDev(int type, int code) {
  base::ScopedFD fd;
  for (int i = 0; i < kMaxInputDev; ++i) {
    fd.reset(open(base::StringPrintf("/dev/input/event%d", i).c_str(),
                  O_RDWR | O_CLOEXEC));
    if (!fd.is_valid())
      break;
    if (HasEventBit(fd.get(), 0, type) && HasEventBit(fd.get(), type, code))
      break;
  }
  return fd;
}

void InjectEvent(const struct input_event& event) {
  base::ScopedFD fd = OpenDev(event.type, event.code);
  if (!fd.is_valid())
    LogErrorExit("No supported input device");

  TEMP_FAILURE_RETRY(write(fd.get(), &event, sizeof(event)));
  TEMP_FAILURE_RETRY(write(fd.get(), &kSync, sizeof(kSync)));
}

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(code, "", "Input event type to inject (one of tablet, lid)");
  DEFINE_int32(value, -1, "Input event value to inject (0 is off, 1 is on)");

  brillo::FlagHelper::Init(argc, argv, "Inject input events to powerd.\n");

  struct input_event event = CreateEvent(FLAGS_code, FLAGS_value);

  InjectEvent(event);
  return 0;
}
