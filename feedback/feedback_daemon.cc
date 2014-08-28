// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feedback/feedback_daemon.h"

#include <unistd.h>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/run_loop.h>
#include <base/strings/string_util.h>
#include "components/feedback/feedback_report.h"
#include "dbus/bus.h"
#include "feedback/feedback_uploader_curl.h"
#include "libchromeos/chromeos/syslog_logging.h"

namespace {
// string, set to "test" to use the sandbox server, otherwise a url
// to send the report to.
const char kSwitchCustomServer[] = "url";

const int kMaxPoolThreads = 1;
const char kPoolName[] = "FeedbackWorkerPool";

const base::FilePath::CharType kFeedbackReportPath[] =
    FILE_PATH_LITERAL("/var/run/");

const char kFeedbackTestUrl[] =
    "http://sandbox.google.com/tools/feedback/chrome/__submit";
const char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";
}  // namespace


namespace feedback {
Daemon::Daemon(const std::string& url)
    : loop_(base::MessageLoop::TYPE_IO),
    pool_(new base::SequencedWorkerPool(kMaxPoolThreads, kPoolName)),
    uploader_(new FeedbackUploaderCurl(base::FilePath(kFeedbackReportPath),
                                       pool_, url)) {}
Daemon::~Daemon() {
  pool_->Shutdown();
}

void Daemon::Run() {
  base::RunLoop loop;
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  options.disconnected_callback = loop.QuitClosure();

  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  scoped_refptr<DbusFeedbackServiceImpl> impl =
      new DbusFeedbackServiceImpl(uploader_.get());

  // Load all reports currently on disk and queue them for sending.
  FeedbackReport::LoadReportsAndQueue(
      uploader_->GetFeedbackReportsPath(),
      base::Bind(&FeedbackService::QueueExistingReport, impl.get()));

  if (!impl->Start(bus)) {
    LOG(FATAL) << "Failed to start feedback service";
  }

  loop.Run();
}
}  // namespace feedback

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* args = CommandLine::ForCurrentProcess();

  // Some libchrome calls need this.
  base::AtExitManager at_exit_manager;

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  std::string url = args->GetSwitchValueASCII(kSwitchCustomServer);
  if (url.empty()) {
    url = kFeedbackPostUrl;
  } else if (!url.compare("test")) {
    url = kFeedbackTestUrl;
    LOG(INFO) << "Using test feedback server";
  } else {
    LOG(INFO) << "Using feedback server at: " << url;
  }

  feedback::Daemon daemon(url);
  daemon.Run();
  return 0;
}
