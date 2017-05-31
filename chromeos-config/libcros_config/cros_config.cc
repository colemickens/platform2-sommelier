// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Libary to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"

// TODO(sjg@chromium.org): See if this can be accepted upstream.
extern "C" {
#include <libfdt.h>
};

#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/string_util.h>

namespace {
const char kConfigDtbPath[] = "/usr/share/chromeos-config/config.dtb";
const char kModelNodePath[] = "/chromeos/models";
}  // namespace

namespace brillo {

CrosConfig::CrosConfig() {}

CrosConfig::~CrosConfig() {}

bool CrosConfig::Init() {
  const base::FilePath::CharType* const argv[] = {"mosys", "platform", "model"};
  base::CommandLine cmdline(arraysize(argv), argv);

  return InitCommon(base::FilePath(kConfigDtbPath), cmdline);
}

bool CrosConfig::InitForTest(const base::FilePath& filepath,
                             const std::string& model) {
  const base::FilePath::CharType* const argv[] = {"echo", model.c_str()};
  base::CommandLine cmdline(arraysize(argv), argv);

  return InitCommon(filepath, cmdline);
}

bool CrosConfig::GetString(const std::string &path, const std::string &prop,
                           std::string* val) {
  if (!inited_) {
    LOG(ERROR) << "Init() must be called before accessing configuration";
    return false;
  }

  // TODO(sjg): Handle non-root nodes.
  if (path != "/") {
    LOG(ERROR) << "Cannot access non-root node " << path;
    return false;
  }

  int len = 0;
  const char* ptr = static_cast<const char*>(
      fdt_getprop(blob_.c_str(), model_offset_, prop.c_str(), &len));
  if (!ptr || len < 0) {
    LOG(WARNING) << "Cannot get path " << path << " property " << prop << ": "
                 << fdt_strerror(len);
    return false;
  }

  // We must have a normally terminated string. This guards against a string
  // list being used, or perhaps a property that does not contain a valid
  // string at all.
  if (!len || strnlen(ptr, len) != static_cast<size_t>(len - 1)) {
    LOG(ERROR) << "String at path " << path << " property " << prop
               << " is invalid";
    return false;
  }

  val->assign(ptr);

  return true;
}

bool CrosConfig::InitCommon(const base::FilePath& filepath,
                            const base::CommandLine& cmdline) {
  // Many systems will not have a config database (yet), so just skip all the
  // setup without any errors if the config file doesn't exist.
  if (!base::PathExists(filepath)) {
    return false;
  }

  std::string output;
  if (!base::GetAppOutput(cmdline, &output)) {
    LOG(ERROR) << "Could not run command " << cmdline.GetCommandLineString();
    return false;
  }
  base::TrimWhitespaceASCII(output, base::TRIM_TRAILING, &model_);

  if (!base::ReadFileToString(filepath, &blob_)) {
    LOG(ERROR) << "Could not read file " << filepath.MaybeAsASCII();
    return false;
  }

  const void* blob = blob_.c_str();
  int ret = fdt_check_header(blob);
  if (ret) {
    LOG(ERROR) << "Config file " << filepath.MaybeAsASCII() << " is invalid: "
               << fdt_strerror(ret);
    return false;
  }
  int node = fdt_path_offset(blob, kModelNodePath);
  if (node < 0) {
    LOG(ERROR) << "Cannot find " << kModelNodePath << " node: "
               << fdt_strerror(node);
    return false;
  }
  node = fdt_subnode_offset(blob, node, model_.c_str());
  if (node < 0) {
    LOG(ERROR) << "Cannot find " << model_ << " node: " << fdt_strerror(node);
    return false;
  }
  model_offset_ = node;
  inited_ = true;
  LOG(INFO) << "Using master configuration for model " << model_;

  return true;
}

}  // namespace brillo
