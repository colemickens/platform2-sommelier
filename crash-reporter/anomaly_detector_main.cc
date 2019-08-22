// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/anomaly_detector.h"

#include <random>
#include <string>

#include <base/at_exit.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <brillo/process.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include <systemd/sd-journal.h>

#include "metrics_event/proto_bindings/metrics_event.pb.h"

// work around https://crbug.com/849450: the LOG_WARNING macro from
// usr/include/sys/syslog.h overrides the LOG_WARNING constant in
// base/logging.h, causing LOG(WARNING) to not compile.
// TODO(https://crbug.com/849450): Remove this once bug is fixed.
#undef LOG_INFO
#undef LOG_WARNING

struct JournalEntry {
  std::string tag;
  std::string message;
  uint64_t monotonic_usec;
};

class Journal {
 public:
  Journal() {
    int ret = sd_journal_open(&j_, SD_JOURNAL_SYSTEM | SD_JOURNAL_LOCAL_ONLY);
    CHECK_GE(ret, 0) << "Could not open journal: " << strerror(-ret);
    // Go directly to the end of the file.  We don't want to parse the same
    // anomalies multiple times on reboot/restart.  We might miss some
    // anomalies, but so be it---it's too hard to keep track reliably of the
    // last parsed position in the syslog.
    SeekToEnd();
    // NOTE: This message is consumed by tast tests to determine when the
    // anomaly detector has started. Don't change it without changing them.
    LOG(INFO) << "Opened journal and sought to end";
  }

  JournalEntry GetNextEntry() {
    MoveToNext();
    auto tag = GetFieldValue("SYSLOG_IDENTIFIER");
    auto message = GetFieldValue("MESSAGE");
    if (tag && message) {
      uint64_t monotonic_usec;
      sd_id128_t ignore;
      int ret = sd_journal_get_monotonic_usec(j_, &monotonic_usec, &ignore);
      CHECK_GE(ret, 0) << "Failed to get monotonic timestamp from journal: "
                       << strerror(-ret);
      return {std::move(*tag), std::move(*message), monotonic_usec};
    } else {
      return GetNextEntry();
    }
  }

 private:
  void SeekToEnd() {
    int ret = sd_journal_seek_tail(j_);
    CHECK_GE(ret, 0) << "Could not seek to end of journal: " << strerror(-ret);
  }

  void MoveToNext() {
    int ret = sd_journal_next(j_);
    CHECK_GE(ret, 0) << "Failed to iterate to next journal entry: "
                     << strerror(-ret);
    if (ret == 0) {
      /* Reached the end, let's wait for changes, and try again. */
      ret = sd_journal_wait(j_, -1 /* timeout */);
      CHECK_GE(ret, 0) << "Failed to wait for journal changes: "
                       << strerror(-ret);
      MoveToNext();
    }
  }

  base::Optional<std::string> GetFieldValue(const std::string& field) {
    const char* data = nullptr;
    size_t length = 0;
    int ret =
        sd_journal_get_data(j_, field.c_str(), (const void**)&data, &length);
    if (ret == -EBADMSG) {
      LOG(WARNING) << "Ignoring corrupt journal entry: " << field;
      return base::nullopt;
    }
    if (ret == -ENOENT)
      return base::nullopt;
    CHECK_GE(ret, 0) << "Failed to read field '" << field
                     << "' from journal: " << strerror(-ret);
    data += field.length() + 1;
    length -= field.length() + 1;

    return std::string(data, length);
  }

  sd_journal* j_ = 0;
};

// Prepares for sending D-Bus signals.  Returns a D-Bus object, which provides
// a handle for sending signals.
scoped_refptr<dbus::Bus> SetUpDBus(void) {
  // Connect the bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> dbus(new dbus::Bus(options));
  CHECK(dbus);
  CHECK(dbus->Connect()) << "Failed to connect to D-Bus";
  return dbus;
}

// Callback to run crash-reporter.
void RunCrashReporter(const std::string& flag, const std::string& input) {
  brillo::ProcessImpl cmd;
  cmd.AddArg("/sbin/crash_reporter");
  cmd.AddArg(flag);
  cmd.RedirectUsingPipe(STDIN_FILENO, true);
  CHECK(cmd.Start());
  int stdin_fd = cmd.GetPipe(STDIN_FILENO);
  CHECK(base::WriteFileDescriptor(stdin_fd, input.data(), input.length()));
  CHECK_GE(close(stdin_fd), 0);
  CHECK_EQ(0, cmd.Wait());
}

std::unique_ptr<dbus::Signal> MakeOomSignal(const int64_t oom_timestamp_ms) {
  auto signal = std::make_unique<dbus::Signal>(
      anomaly_detector::kAnomalyEventServiceInterface,
      anomaly_detector::kAnomalyEventSignalName);
  dbus::MessageWriter writer(signal.get());
  metrics_event::Event payload;
  payload.set_type(metrics_event::Event_Type_OOM_KILL_KERNEL);
  payload.set_timestamp(oom_timestamp_ms);
  writer.AppendProtoAsArrayOfBytes(payload);

  return signal;
}

int main(int argc, char* argv[]) {
  // Sim sala bim!  These are needed to send D-Bus signals.  Even though they
  // are not used directly, they set up some global state needed by the D-Bus
  // library.
  base::MessageLoop message_loop;
  base::AtExitManager at_exit_manager;

  brillo::OpenLog("anomaly_detector", true);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  scoped_refptr<dbus::Bus> dbus = SetUpDBus();
  // Export a bus object so that other processes can register signal handlers
  // (this service only sends signals, no methods are exported).
  dbus::ExportedObject* exported_object = dbus->GetExportedObject(
      dbus::ObjectPath(anomaly_detector::kAnomalyEventServicePath));
  CHECK(exported_object);

  // We only want to report 0.1% of selinux violations. Set up the random
  // distribution.
  std::default_random_engine gen((std::random_device())());
  std::bernoulli_distribution drop_report(0.999);

  Journal j;

  std::map<std::string, std::unique_ptr<anomaly::Parser>> parsers;
  parsers["audit"] = std::make_unique<anomaly::SELinuxParser>();
  parsers["init"] = std::make_unique<anomaly::ServiceParser>();
  parsers["kernel"] = std::make_unique<anomaly::KernelParser>();

  while (true) {
    JournalEntry entry = j.GetNextEntry();
    if (parsers.count(entry.tag) > 0) {
      auto crash_report = parsers[entry.tag]->ParseLogEntry(entry.message);
      if (crash_report) {
        if (entry.tag == "audit" && drop_report(gen))
          continue;
        RunCrashReporter(crash_report->flag, crash_report->text);
      }
    }

    // Handle OOM messages.
    if (entry.tag == "kernel" &&
        entry.message.find("Out of memory: Kill process") != std::string::npos)
      exported_object->SendSignal(
          MakeOomSignal(entry.monotonic_usec / 1000).get());
  }
}
