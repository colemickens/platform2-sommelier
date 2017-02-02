// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics/metrics_library.h"

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstring>

#include "metrics/serialization/metric_sample.h"
#include "metrics/serialization/serialization_utils.h"

#include "policy/device_policy.h"

static const char kUMAEventsPath[] = "/var/lib/metrics/uma-events";
static const char kConsentFile[] = "/home/chronos/Consent To Send Stats";
static const char kCrosEventHistogramName[] = "Platform.CrOSEvent";
static const int kCrosEventHistogramMax = 100;

// Add new cros events here.
//
// The index of the event is sent in the message, so please do not
// reorder the names.
//
// Note: All updates here must also update Chrome's historgram.xml database.
// Please see this document for more details:
// https://chromium.googlesource.com/chromium/src/+/master/tools/metrics/histograms/
//
// You can view them live here:
// https://uma.googleplex.com/histograms/?histograms=Platform.CrOSEvent
static const char *kCrosEventNames[] = {
  "ModemManagerCommandSendFailure",  // 0
  "HwWatchdogReboot",  // 1
  "Cras.NoCodecsFoundAtBoot",  // 2
  "Chaps.DatabaseCorrupted",  // 3
  "Chaps.DatabaseRepairFailure",  // 4
  "Chaps.DatabaseCreateFailure",  // 5
  "Attestation.OriginSpecificExhausted",  // 6
  "SpringPowerSupply.Original.High",  // 7
  "SpringPowerSupply.Other.High",  // 8
  "SpringPowerSupply.Original.Low",  // 9
  "SpringPowerSupply.ChargerIdle",  // 10
  "TPM.NonZeroDictionaryAttackCounter",  // 11
  "TPM.EarlyResetDuringCommand",  // 12
  "VeyronEmmcUpgrade.Success",  // 13
  "VeyronEmmcUpgrade.WaitForKernelRollup",  // 14
  "VeyronEmmcUpgrade.WaitForFirmwareRollup",  // 15
  "VeyronEmmcUpgrade.BadEmmcProperties",  // 16
  "VeyronEmmcUpgrade.FailedDiskAccess",  // 17
  "VeyronEmmcUpgrade.FailedWPEnable",  // 18
  "VeyronEmmcUpgrade.SignatureDetected",  // 19
  "Watchdog.StartupFailed",  // 20
};

time_t MetricsLibrary::cached_enabled_time_ = 0;
bool MetricsLibrary::cached_enabled_ = false;

MetricsLibrary::MetricsLibrary() : consent_file_(kConsentFile) {}
MetricsLibrary::~MetricsLibrary() {}

// We take buffer and buffer_size as parameters in order to simplify testing
// of various alignments of the |device_name| with |buffer_size|.
bool MetricsLibrary::IsDeviceMounted(const char* device_name,
                                     const char* mounts_file,
                                     char* buffer,
                                     int buffer_size,
                                     bool* result) {
  if (buffer == nullptr || buffer_size < 1)
    return false;
  int mounts_fd = open(mounts_file, O_RDONLY);
  if (mounts_fd < 0)
    return false;
  // match_offset describes:
  //   -1 -- not beginning of line
  //   0..strlen(device_name)-1 -- this offset in device_name is next to match
  //   strlen(device_name) -- matched full name, just need a space.
  int match_offset = 0;
  bool match = false;
  while (!match) {
    int read_size = read(mounts_fd, buffer, buffer_size);
    if (read_size <= 0) {
      if (errno == -EINTR)
        continue;
      break;
    }
    for (int i = 0; i < read_size; ++i) {
      if (buffer[i] == '\n') {
        match_offset = 0;
        continue;
      }
      if (match_offset < 0) {
        continue;
      }
      if (device_name[match_offset] == '\0') {
        if (buffer[i] == ' ') {
          match = true;
          break;
        }
        match_offset = -1;
        continue;
      }

      if (buffer[i] == device_name[match_offset]) {
        ++match_offset;
      } else {
        match_offset = -1;
      }
    }
  }
  close(mounts_fd);
  *result = match;
  return true;
}

bool MetricsLibrary::IsGuestMode() {
  char buffer[256];
  bool result = false;
  if (!IsDeviceMounted("guestfs",
                       "/proc/mounts",
                       buffer,
                       sizeof(buffer),
                       &result)) {
    return false;
  }
  return result && (access("/run/state/logged-in", F_OK) == 0);
}

bool MetricsLibrary::ConsentId(std::string* id) {
  // Do not allow symlinks.
  base::ScopedFD fd(open(consent_file_.c_str(), O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
  if (fd.get() < 0)
    return false;

  // We declare a slightly larger buffer than needed so we can detect if it's
  // been corrupted with a lot of bad data.
  char buf[40];
  ssize_t len = read(fd.get(), buf, sizeof(buf));

  // If we couldn't get any data, just fail right away.
  if (len <= 0)
    return false;

  // Chop the trailing newline to make parsing below easier.
  if (buf[len - 1] == '\n')
    buf[--len] = '\0';

  // Make sure it's a valid UUID.  Support older installs that omitted dashes.
  if (len != 32 && len != 36)
    return false;

  ssize_t i;
  id->clear();
  for (i = 0; i < len; ++i) {
    char c = buf[i];
    *id += c;

    // For long UUIDs, require dashes at certain positions.
    if (len == 36 && (i == 8 || i == 13 || i == 18 || i == 23)) {
      if (c == '-')
        continue;
      return false;
    }

    // All the rest should be hexdigits.
    if (base::IsHexDigit(c))
      continue;

    return false;
  }

  return true;
}

bool MetricsLibrary::AreMetricsEnabled() {
  time_t this_check_time = time(nullptr);
  if (this_check_time != cached_enabled_time_) {
    cached_enabled_time_ = this_check_time;

    if (!policy_provider_.get())
      policy_provider_.reset(new policy::PolicyProvider());
    policy_provider_->Reload();
    // We initialize with the default value which is false and will be preserved
    // if the policy is not set.
    bool enabled = false;
    bool has_policy = false;
    if (policy_provider_->device_policy_is_loaded()) {
      has_policy =
          policy_provider_->GetDevicePolicy().GetMetricsEnabled(&enabled);
    }
    // If policy couldn't be loaded or the metrics policy is not set we should
    // still respect the consent file if it is present for migration purposes.
    // TODO(pastarmovj)
    if (!has_policy) {
      std::string id;
      enabled = ConsentId(&id);
    }

    if (enabled && !IsGuestMode())
      cached_enabled_ = true;
    else
      cached_enabled_ = false;
  }
  return cached_enabled_;
}

bool MetricsLibrary::EnableMetrics() {
  // Already enabled? Don't touch anything.
  if (AreMetricsEnabled())
    return true;

  std::string guid = base::GenerateGUID();

  if (guid.empty())
    return false;

  // http://crbug.com/383003 says we must be world readable.
  mode_t mask = umask(0022);
  int write_len = base::WriteFile(base::FilePath(consent_file_),
                                  guid.c_str(), guid.length());
  umask(mask);

  return write_len == static_cast<int>(guid.length());
}

bool MetricsLibrary::DisableMetrics() {
  return base::DeleteFile(base::FilePath(consent_file_), false);
}

void MetricsLibrary::Init() {
  uma_events_file_ = kUMAEventsPath;
}

bool MetricsLibrary::SendToUMA(const std::string& name,
                               int sample,
                               int min,
                               int max,
                               int nbuckets) {
  return metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::HistogramSample(name, sample, min, max, nbuckets)
           .get(),
      kUMAEventsPath);
}

bool MetricsLibrary::SendEnumToUMA(const std::string& name, int sample,
                                   int max) {
  return metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::LinearHistogramSample(name, sample, max).get(),
      kUMAEventsPath);
}

bool MetricsLibrary::SendBoolToUMA(const std::string& name, bool sample) {
  return metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::LinearHistogramSample(name,
                                                    sample ? 1 : 0, 2).get(),
      kUMAEventsPath);
}

bool MetricsLibrary::SendSparseToUMA(const std::string& name, int sample) {
  return metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::SparseHistogramSample(name, sample).get(),
      kUMAEventsPath);
}

bool MetricsLibrary::SendUserActionToUMA(const std::string& action) {
  return metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::UserActionSample(action).get(), kUMAEventsPath);
}

bool MetricsLibrary::SendCrashToUMA(const char *crash_kind) {
  return metrics::SerializationUtils::WriteMetricToFile(
      *metrics::MetricSample::CrashSample(crash_kind).get(), kUMAEventsPath);
}

void MetricsLibrary::SetPolicyProvider(policy::PolicyProvider* provider) {
  policy_provider_.reset(provider);
}

bool MetricsLibrary::SendCrosEventToUMA(const std::string& event) {
  for (size_t i = 0; i < arraysize(kCrosEventNames); i++) {
    if (strcmp(event.c_str(), kCrosEventNames[i]) == 0) {
      return SendEnumToUMA(kCrosEventHistogramName, i, kCrosEventHistogramMax);
    }
  }
  return false;
}
