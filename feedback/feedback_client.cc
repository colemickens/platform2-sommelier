// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feedback/feedback_daemon.h"

#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "brillo/process.h"
#include "brillo/syslog_logging.h"
#include "components/feedback/feedback_common.h"
#include "feedback/feedback_service_interface.h"

#include <sysexits.h>

namespace {

static const char kSwitchProductId[] = "product_id";  // int
static const char kSwitchDescription[] = "desc";  // string
static const char kSwitchBucket[] = "bucket";  // string
static const char kSwitchUserEmail[] = "user_email";  // string
static const char kSwitchPageUrl[] = "page_url";  // string
static const char kSwitchRawFiles[] = "raw_files";  // colon-separated strings

const char kListSeparator[] = ":";

void CommandlineReportStatus(base::WaitableEvent* event, bool* status,
                             bool result) {
  *status = result;
  event->Signal();
}

bool FillReportFromCommandline(FeedbackCommon* report) {
  base::CommandLine* args = base::CommandLine::ForCurrentProcess();
  if (!args->HasSwitch(kSwitchProductId)) {
    LOG(ERROR) << "No product id provided";
    return false;
  }
  std::string product_id_string = args->GetSwitchValueASCII(kSwitchProductId);
  int product_id;
  if (!base::StringToInt(product_id_string, &product_id) || product_id <= 0) {
    LOG(ERROR) << "Invalid product id provided, must be a positive number";
    return false;
  }
  if (!args->HasSwitch(kSwitchDescription)) {
    LOG(ERROR) << "No description provided";
    return false;
  }

  report->AddLog("unique_guid", base::GenerateGUID());
  report->set_product_id(product_id);
  report->set_description(args->GetSwitchValueASCII(kSwitchDescription));
  report->set_user_email(args->GetSwitchValueASCII(kSwitchUserEmail));
  report->set_page_url(args->GetSwitchValueASCII(kSwitchPageUrl));
  report->set_category_tag(args->GetSwitchValueASCII(kSwitchBucket));

  std::vector<std::string> raw_files =
      base::SplitString(args->GetSwitchValueNative(kSwitchRawFiles),
                        kListSeparator, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& path : raw_files) {
    auto content = std::make_unique<std::string>();
    if (base::ReadFileToString(base::FilePath(path), content.get())) {
      report->AddFile(path, std::move(content));
    } else {
      LOG(ERROR) << "Could not read raw file: " << path;
      return false;
    }
  }

  std::vector<std::string> log_files = args->GetArgs();
  for (const std::string& path : log_files) {
    std::string content;
    if (base::ReadFileToString(base::FilePath(path), &content)) {
      report->AddLog(path, content);
    } else {
      LOG(ERROR) << "Could not read log file: " << path;
      return false;
    }
  }
  return true;
}

bool SendReport(FeedbackServiceInterface* interface, FeedbackCommon* report) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  bool status;

  report->CompressLogs();
  interface->SendFeedback(*report, base::Bind(&CommandlineReportStatus,
                                              &event, &status));
  event.Wait();
  return status;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // Some libchrome calls need this.
  base::AtExitManager at_exit_manager;

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  scoped_refptr<FeedbackServiceInterface> itf =
      new DBusFeedbackServiceInterface();
  scoped_refptr<FeedbackCommon> report = new FeedbackCommon();

  if (!FillReportFromCommandline(report.get())) {
    LOG(ERROR) << "Not sending report";
    return EX_USAGE;
  }

  return SendReport(itf.get(), report.get()) ? EX_OK : EX_UNAVAILABLE;
}
