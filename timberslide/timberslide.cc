// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <poll.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/flag_helper.h>

namespace {

const char kDefaultDeviceLogFile[] = "/sys/kernel/debug/cros_ec/console_log";
const char kDefaultDeviceUptimeFile[] = "/sys/kernel/debug/cros_ec/uptime";
const char kDefaultLogDirectory[] = "/var/log/";
const char kCurrentLogExt[] = ".log";
const char kPreviousLogExt[] = ".previous";
const int kMaxCurrentLogSize = 10 * 1024 * 1024;

// String proxy class for dealing with lines.
class LineExtractor {
 public:
  // Extract one line from beginning of string stream.
  friend std::istream& operator>>(std::istream& is, LineExtractor& l) {
    std::getline(is, l.str_);
    return is;
  }

  // Transparently convert to std::string.
  operator std::string() const { return str_; }

 private:
  std::string str_;
};

//
// Has a member function which adds the host timestamp
// to the beginning of each line passed to it.
//
class StringTransformer {
 public:
  explicit StringTransformer(int64_t ec_uptime_ms)
      : ec_current_uptime_ms_(ec_uptime_ms), timestamp_(base::Time::Now()) {}

  // Matching lines look like: [1234.5678 EC message goes here] .
  std::string add_host_ts(const std::string& s) {
    const base::TimeDelta ec_sync(
        base::TimeDelta::FromMilliseconds(ec_current_uptime_ms_));

    std::string::size_type bracket = s.find('[');
    if (bracket == std::string::npos)
      return s;

    std::string::size_type space = s.find(' ', bracket + 1);
    if (space == std::string::npos)
      return s;

    double ec_ts;
    const std::string potential_ts(s.substr(bracket + 1, space - bracket - 1));

    if (!base::StringToDouble(potential_ts, &ec_ts)) {
      LOG(WARNING) << "Unable to convert " << potential_ts << " to a double";
      return s;
    }

    // Calculate delta from EC's uptime.
    const base::TimeDelta logline_tm(base::TimeDelta::FromSecondsD(ec_ts));
    const base::TimeDelta logline_delta(ec_sync - logline_tm);
    const base::Time logline_host_tm = timestamp_ - logline_delta;

    return FormatTime(logline_host_tm).append(" ").append(s);
  }

 private:
  std::string FormatTime(const base::Time& time) {
    base::Time::Exploded e;

    // This format matches the format in libchrome/base/logging.cc
    time.UTCExplode(&e);
    return base::StringPrintf("%02d%02d/%02d%02d%02d.%06d", e.month,
                              e.day_of_month, e.hour, e.minute, e.second,
                              e.millisecond * 1000);
  }

  int64_t ec_current_uptime_ms_;
  base::Time timestamp_;
};

class TimberSlide : public brillo::Daemon,
                    public base::MessageLoopForIO::Watcher {
 public:
  TimberSlide(const std::string& ec_type,
              base::File device_file,
              base::File uptime_file,
              const base::FilePath& log_dir)
      : device_file_(std::move(device_file)),
        fd_watcher_(FROM_HERE),
        total_size_(0),
        uptime_file_(std::move(uptime_file)),
        uptime_file_valid_(uptime_file_.IsValid()) {
    current_log_ = log_dir.Append(ec_type + kCurrentLogExt);
    previous_log_ = log_dir.Append(ec_type + kPreviousLogExt);
  }

 private:
  int OnInit() override {
    LOG(INFO) << "Starting timberslide daemon";
    int ret = brillo::Daemon::OnInit();
    if (ret != EX_OK)
      return ret;

    if (uptime_file_valid_)
      LOG(INFO) << "EC uptime file is valid";
    else
      LOG(WARNING) << "EC uptime file is not valid; ignoring";

    RotateLogs(previous_log_, current_log_);

    CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
        device_file_.GetPlatformFile(), true,
        base::MessageLoopForIO::WATCH_READ, &fd_watcher_, this));
    return EX_OK;
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    LOG(FATAL) << "Unexpected call to write event handler";
  }

  //
  // From kernel's Documentation/filesystems/sysfs.txt: If userspace seeks back
  // to zero or does a pread(2) with an offset of '0' the show() method will
  // be called again, rearmed, to fill the buffer.
  //
  // Therefore, the 'uptime' file will be kept open and just seeked back to
  // 0 when new uptime is needed.
  //
  bool GetEcUptime(int64_t* ec_uptime_ms) {
    char uptime_buf[64] = {0};

    if (!uptime_file_valid_ ||
        uptime_file_.Seek(base::File::FROM_BEGIN, 0) != 0)
      return false;

    // Read single line from file and parse as a number.
    int count =
        uptime_file_.ReadAtCurrentPos(uptime_buf, sizeof(uptime_buf) - 1);

    if (count <= 0)
      return false;

    uptime_buf[count] = '\0';
    base::StringToInt64(uptime_buf, ec_uptime_ms);

    // If the 'uptime' file contains zero, that means the kernel patch is
    // available, but the EC doesn't support EC_CMD_GET_UPTIME_INFO.  In
    // that case, this returns false so that incorrect times aren't reported
    // in the EC log file.
    return (*ec_uptime_ms > 0);
  }

  void OnFileCanReadWithoutBlocking(int fd) override {
    char buffer[4096];
    int ret;
    int64_t ec_current_uptime_ms = 0;

    CHECK_EQ(fd, device_file_.GetPlatformFile());
    memset(buffer, 0, sizeof(buffer));

    ret = TEMP_FAILURE_RETRY(
        device_file_.ReadAtCurrentPosNoBestEffort(buffer, sizeof(buffer)));
    if (ret == 0)
      return;

    if (ret < 0) {
      PLOG(ERROR) << "Read error";
      Quit();
      return;
    }

    std::string str;
    if (GetEcUptime(&ec_current_uptime_ms)) {
      StringTransformer xfrm(ec_current_uptime_ms);
      auto fn_xfrm = std::bind(&StringTransformer::add_host_ts, &xfrm,
                               std::placeholders::_1);
      std::istringstream iss(buffer);
      std::ostringstream oss;

      // Iterate over each line and prepend the corresponding host timestamp
      std::transform(std::istream_iterator<LineExtractor>(iss),
                     std::istream_iterator<LineExtractor>(),
                     std::ostream_iterator<std::string>(oss, "\n"), fn_xfrm);

      str = oss.str();
    } else {
      // ret is the number of characters read from the device_file_
      str = std::string(buffer, ret);
    }

    ret = str.size();

    if (!base::AppendToFile(current_log_, str.c_str(), ret)) {
      PLOG(ERROR) << "Could not append to log file";
      Quit();
      return;
    }

    total_size_ += ret;
    if (total_size_ >= kMaxCurrentLogSize) {
      RotateLogs(previous_log_, current_log_);
      total_size_ = 0;
    }
  }

  void RotateLogs(const base::FilePath& previous_log,
                  const base::FilePath& current_log) {
    CHECK(base::DeleteFile(previous_log, /* recursive = */ false));

    if (base::PathExists(current_log))
      CHECK(base::Move(current_log, previous_log));

    base::WriteFile(current_log, "", 0);
  }

  base::File device_file_;
  base::FilePath current_log_;
  base::FilePath previous_log_;
  base::MessageLoopForIO::FileDescriptorWatcher fd_watcher_;
  int total_size_;
  base::File uptime_file_;
  bool uptime_file_valid_;
};

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_string(device_log, kDefaultDeviceLogFile,
                "File where the recent EC logs are posted to.");
  DEFINE_string(log_directory, kDefaultLogDirectory,
                "Directory where the output logs should be.");
  DEFINE_string(uptime_file, kDefaultDeviceUptimeFile,
                "Device uptime file.");
  brillo::FlagHelper::Init(
      argc, argv, "timberslide concatenates EC logs for use in debugging.");

  const base::FilePath path(FLAGS_device_log);
  base::File device_file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!device_file.IsValid()) {
    LOG(ERROR) << "Error opening " << FLAGS_device_log << ": "
               << base::File::ErrorToString(device_file.error_details());
    return EX_UNAVAILABLE;
  }

  const base::FilePath uptime_path(FLAGS_uptime_file);
  base::File uptime_file(uptime_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);

  std::string ec_type = path.DirName().BaseName().value();
  TimberSlide ts(ec_type, std::move(device_file), std::move(uptime_file),
                 base::FilePath(FLAGS_log_directory));

  return ts.Run();
}
