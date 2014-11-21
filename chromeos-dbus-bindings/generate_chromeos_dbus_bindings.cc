// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <chromeos/syslog_logging.h>

#include "chromeos-dbus-bindings/adaptor_generator.h"
#include "chromeos-dbus-bindings/method_name_generator.h"
#include "chromeos-dbus-bindings/proxy_generator.h"
#include "chromeos-dbus-bindings/xml_interface_parser.h"

using chromeos_dbus_bindings::AdaptorGenerator;
using chromeos_dbus_bindings::MethodNameGenerator;
using chromeos_dbus_bindings::ProxyGenerator;

namespace switches {

static const char kHelp[] = "help";
static const char kMethodNames[] = "method-names";
static const char kAdaptor[] = "adaptor";
static const char kProxy[] = "proxy";
static const char kHelpMessage[] = "\n"
    "generate-chromeos-dbus-bindings itf1.xml [itf2.xml...] [switches]\n"
    "    itf1.xml, ... = the input interface file(s) [mandatory].\n"
    "Available Switches: \n"
    "  --method-names=<method name header filename>\n"
    "    The output header file with string constants for each method name.\n"
    "  --adaptor=<adaptor header filename>\n"
    "    The output header file name containing the DBus adaptor class.\n"
    "  --proxy=<proxy header filename>\n"
    "    The output header file name containing the DBus proxy class.\n";

}  // namespace switches

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  // Setup logging to stderr. This also parses some implicit flags using the
  // CommandLine singleton.
  chromeos::InitLog(chromeos::kLogToStderr | chromeos::kLogHeader);

  if (cl->HasSwitch(switches::kHelp)) {
    LOG(INFO) << switches::kHelpMessage;
    return 0;
  }

  auto input_files = cl->GetArgs();
  if (input_files.empty()) {
    LOG(ERROR) << "At least one file must be specified.";
    LOG(ERROR) << switches::kHelpMessage;
    return 1;
  }

  chromeos_dbus_bindings::XmlInterfaceParser parser;
  for (const auto& input : input_files) {
    if (!parser.ParseXmlInterfaceFile(base::FilePath(input))) {
      LOG(ERROR) << "Failed to parse interface file.";
      return 1;
    }
  }

  if (cl->HasSwitch(switches::kMethodNames)) {
    std::string method_name_file =
        cl->GetSwitchValueASCII(switches::kMethodNames);
    VLOG(1) << "Outputting method names to " << method_name_file;
    if (!MethodNameGenerator::GenerateMethodNames(
            parser.interfaces(),
            base::FilePath(method_name_file))) {
      LOG(ERROR) << "Failed to output method names.";
      return 1;
    }
  }

  if (cl->HasSwitch(switches::kAdaptor)) {
    std::string adaptor_file = cl->GetSwitchValueASCII(switches::kAdaptor);
    // GYP sometimes enclosed the target file name in extra set of quotes like:
    // generate-chromeos-dbus-bindings in.xml "--adaptor=\"out.h\""
    base::TrimString(adaptor_file, "\"'", &adaptor_file);
    VLOG(1) << "Outputting adaptor to " << adaptor_file;
    if (!AdaptorGenerator::GenerateAdaptors(parser.interfaces(),
                                            base::FilePath(adaptor_file))) {
      LOG(ERROR) << "Failed to output adaptor.";
      return 1;
     }
  }

  if (cl->HasSwitch(switches::kProxy)) {
    std::string proxy_file = cl->GetSwitchValueASCII(switches::kProxy);
    // GYP sometimes enclosed the target file name in extra set of quotes like:
    // generate-chromeos-dbus-bindings in.xml "--proxy=\"out.h\""
    base::TrimString(proxy_file, "\"'", &proxy_file);
    VLOG(1) << "Outputting proxy to " << proxy_file;
    if (!ProxyGenerator::GenerateProxies(parser.interfaces(),
                                         base::FilePath(proxy_file))) {
      LOG(ERROR) << "Failed to output proxy.";
      return 1;
     }
  }

  return 0;
}
