// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <brillo/flag_helper.h>

namespace {

constexpr char kDevInputEvent[] = "/dev/input";
constexpr char kEventDevGlob[] = "event*";

// Determines if the given |bit| is set in the |bitmask| array.
bool TestBit(const int bit, const uint8_t* bitmask) {
  return (bitmask[bit / 8] >> (bit % 8)) & 1;
}

bool IsUSBDevice(const int fd) {
  struct input_id id;
  ioctl(fd, EVIOCGID, &id);

  return id.bustype == BUS_USB;
}

bool IsKeyboardDevice(const int fd) {
  uint8_t evtype_bitmask[EV_MAX / 8 + 1];
  ioctl(fd, EVIOCGBIT(0, sizeof(evtype_bitmask)), evtype_bitmask);

  // The device is a "keyboard" if it supports EV_KEY events. Though, it is not
  // necessarily a real keyboard; EV_KEY events could also be e.g. volume
  // up/down buttons on a device.
  return TestBit(EV_KEY, evtype_bitmask);
}

bool SupportsAllKeys(const int fd, const std::vector<int>& events) {
  uint8_t key_bitmask[KEY_MAX / 8 + 1];
  ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bitmask)), key_bitmask);

  for (auto event : events) {
    if (!TestBit(event, key_bitmask)) {
      return false;
    }
  }

  return true;
}

int WaitForKeys(const int fd, const std::vector<int>& events) {
  // Boolean array to keep track of whether a key is currently up or down.
  bool key_states[KEY_MAX + 1] = {false};

  while (true) {
    struct input_event ev;
    int rd = read(fd, &ev, sizeof(ev));
    if (rd < sizeof(struct input_event)) {
      PLOG(ERROR) << "Reading event failed";
      exit(EXIT_FAILURE);
    }

    // A keyboard device may generate events other than EV_KEY, so we should
    // explicitly check here. Also explicitly check |ev.code| is in range, just
    // in case.
    if (ev.type == EV_KEY && ev.code <= KEY_MAX &&
        find(events.begin(), events.end(), ev.code) != events.end()) {
      // We need to perform a bit of extra logic to handle buttons that may have
      // already been pressed when we entered recovery. For example, if a user
      // is holding down their volume keys as they enter recovery, then the key
      // repeat event will get fed into here, and we don't want to act on it
      // since it does not constitute acknowledgment.
      //
      // So, we force that we must have seen the key be pressed and then
      // released in the time that we have been in recovery.
      if (ev.value == 0 && key_states[ev.code]) {
        // Key was released while we knew it was pressed; we're done.
        return ev.code;
      } else if (ev.value == 1) {
        // Only count first presses, long holds/key repeats from entering
        // recovery will have |ev.value| == 2, so won't go down here.
        key_states[ev.code] = true;
      }
    }
  }

  NOTREACHED();
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_bool(check, false,
              "Checks if the requested keys are available, exits with an error "
              "if they are not");
  DEFINE_bool(include_usb, false,
              "Whether USB devices should be scanned for inputs");
  DEFINE_string(keys, "", "Colon-separated list of keycodes to listen for");

  brillo::FlagHelper::Init(
      argc, argv,
      "evwaitkey\n"
      "\n"
      "This utility allows waiting on arbitrary key inputs to a device's\n"
      "primary keyboard. It's primarily intended for use from\n"
      "non-interactive scripts that must obtain user input, e.g.\n"
      "physical presence checks in the recovery installer.\n"
      "\n"
      "It takes at least one key code (as determined by evtest) as input\n"
      "and prints the first key in the given list that was pressed by the\n"
      "user. It may block indefinitely if no key was pressed.\n"
      "\n"
      "Example usage (waiting either for escape key code 1 or enter key "
      "code 28):\n"
      "\n"
      "    $ evwaitkey --keys=1:28\n"
      "    <user presses enter>\n"
      "    28\n"
      "\n"
      "Example usage in script:\n"
      "\n"
      "    KEY_ESC=1\n"
      "    KEY_ENTER=28\n"
      "\n"
      "    if [ $(evwaitkey --keys=$KEY_ESC:$KEY_ENTER) = $KEY_ESC ]; "
      "then\n"
      "      echo \"Escape pressed\"\n"
      "    else\n"
      "      echo \"Enter pressed\"\n"
      "    fi\n");

  auto keys = base::SplitString(FLAGS_keys, ":", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_ALL);

  std::vector<int> events;
  for (const auto& key : keys) {
    int event;
    if (!base::StringToInt(key, &event) || event < 0 || event > KEY_MAX) {
      LOG(ERROR) << "'" << key << "' is not a valid keycode";
      return EXIT_FAILURE;
    }

    events.push_back(event);
  }

  base::FileEnumerator devs(base::FilePath(kDevInputEvent), false,
                            base::FileEnumerator::FILES, kEventDevGlob);

  for (auto ev_dev = devs.Next(); !ev_dev.empty(); ev_dev = devs.Next()) {
    base::ScopedFD fd(open(ev_dev.value().c_str(), O_RDONLY | O_CLOEXEC));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Open event device failed";
      return EXIT_FAILURE;
    }

    // Listen on the first device that matches the event list.
    //
    // In the case of recovery, we should be ignoring input events from external
    // keyboards, since USB-attached devices can be tampered with by a remote
    // attacker to masquerade as keyboards and bypass physical presence checks.
    if ((FLAGS_include_usb || !IsUSBDevice(fd.get())) &&
        IsKeyboardDevice(fd.get()) && SupportsAllKeys(fd.get(), events)) {
      if (!FLAGS_check) {
        int ev = WaitForKeys(fd.get(), events);
        printf("%d\n", ev);
      }

      return EXIT_SUCCESS;
    }
  }

  if (!FLAGS_check) {
    LOG(ERROR) << "could not find device supporting requested keys";
  }

  return EXIT_FAILURE;
}
