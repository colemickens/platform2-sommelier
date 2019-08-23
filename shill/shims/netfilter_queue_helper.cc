// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// netfilter_queue_helper is a user-space process that allows unicast
// replies to multicast requests.  It does so by monitoring output
// multicast packets on one NFQUEUE netlink iptables rule and collating
// a list of input ports that are sending out multicast requests.  It
// uses these results to set policy on incoming UDP packets on a separate
// NFQUEUE for replies addressed to that list of ports.
//
// Expected usage:
//     iptables -I OUTPUT 1 --proto udp
//         --destination <destination_multicast_address> --dport <dport>
//         -j NFQUEUE --queue-num <output_queue_number>
//     iptables -A INPUT --proto udp -j NFQUEUE --queue-num <input_queue_number>
//     netfilter_queue_helper --input-queue=<input_queue_number>
//         --output-queue=<output_queue_number>
//
// Note: in the above example, we preprend the OUTPUT rule so that it
// runs even if lower rules would have accepted it, while the INPUT
// rule is placed at the end of the rule list so any other firewall
// rules that would have accepted the input packet for other reasons
// will be evaluated first so we don't have to involve userspace for them.

#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/syslog_logging.h>

#include "shill/shims/netfilter_queue_processor.h"

using std::string;

namespace switches {

static const char kHelp[] = "help";
static const char kInputQueue[] = "input-queue";
static const char kOutputQueue[] = "output-queue";
static const char kVerbose[] = "verbose";

// The help message shown if help flag is passed to the program.
static const char kHelpMessage[] =
    "\n"
    "Available Switches:\n"
    "  --help\n"
    "    Show this help message.\n"
    "  --input-queue=<input queue number>\n"
    "    Set the netfilter queue number for incoming UDP packets.\n"
    "  --output-queue=<output queue number>\n"
    "    Set the netfilter queue number for outgoing UDP packets for which\n"
    "    input replies will be enabled.\n"
    "  --verbose\n"
    "    Show debug messages.\n";

}  // namespace switches

bool GetIntegerOption(base::CommandLine* cl, const string& option, int* value) {
  if (!cl->HasSwitch(option)) {
    LOG(ERROR) << "Option " << option << " was not given.";
    return false;
  }
  string option_string_value = cl->GetSwitchValueASCII(option);
  int option_integer_value = -1;

  if (!base::StringToInt(option_string_value, &option_integer_value)) {
    LOG(ERROR) << "Unable to convert parameter \"" << option_string_value
               << "\" passed as option " << option << " into an integer.";
    return false;
  }
  *value = option_integer_value;
  return true;
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  int input_queue = -1;
  if (!GetIntegerOption(cl, switches::kInputQueue, &input_queue) ||
      input_queue < 0) {
    LOG(ERROR) << "Unable to get mandatory input queue option.";
    return 1;
  }

  int output_queue = -1;
  if (!GetIntegerOption(cl, switches::kOutputQueue, &output_queue) ||
      output_queue < 0) {
    LOG(ERROR) << "Unable to get mandatory output queue option.";
    return 1;
  }

  if (cl->HasSwitch(switches::kVerbose)) {
    logging::SetMinLogLevel(-3);
  }

  if (output_queue == input_queue) {
    LOG(ERROR) << "Input and output queues must not be the same.";
    return 1;
  }

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader);

  shill::shims::NetfilterQueueProcessor processor(input_queue, output_queue);

  if (!processor.Start()) {
    LOG(ERROR) << "Failed to start netfilter processor.";
    return 1;
  }

  processor.Run();

  return 0;
}
