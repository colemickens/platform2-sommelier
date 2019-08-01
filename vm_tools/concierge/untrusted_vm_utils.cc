// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/untrusted_vm_utils.h"

#include <sys/utsname.h>

#include <string>
#include <utility>
#include <vector>

#include <base/strings/string_split.h>
#include <base/strings/string_number_conversions.h>
#include <base/files/file_util.h>
#include <base/strings/string_piece.h>

namespace vm_tools {
namespace concierge {

namespace {

// Gets the kernel version of the host it's run on. Returns true if retrieved
// successfully, false otherwise.
base::Optional<UntrustedVMUtils::KernelVersionAndMajorRevision>
GetKernelVersion() {
  struct utsname buf;
  if (uname(&buf))
    return base::nullopt;

  // Parse uname result in the form of x.yy.zzz. The parsed data should be in
  // the expected format.
  std::vector<base::StringPiece> versions = base::SplitStringPiece(
      buf.release, ".", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_ALL);
  DCHECK_EQ(versions.size(), 3);
  DCHECK(!versions[0].empty());
  DCHECK(!versions[1].empty());
  int version;
  bool result = base::StringToInt(versions[0], &version);
  DCHECK(result);
  int major_revision;
  result = base::StringToInt(versions[1], &major_revision);
  DCHECK(result);
  return std::make_pair(version, major_revision);
}

// Returns the L1TF mitigation status of the host it's run on.
UntrustedVMUtils::MitigationStatus GetL1TFMitigationStatus(
    const base::FilePath& l1tf_status_path) {
  std::string l1tf_status;
  if (!base::ReadFileToString(l1tf_status_path, &l1tf_status)) {
    LOG(ERROR) << "Failed to read L1TF status";
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;
  }

  std::vector<base::StringPiece> l1tf_statuses = base::SplitStringPiece(
      l1tf_status, ",;", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // The sysfs file should always return up to 3 statuses and no more.
  DCHECK_LE(l1tf_statuses.size(), 3);

  const base::StringPiece& processor_mitigation_status = l1tf_statuses[0];
  if (processor_mitigation_status == "Not affected")
    return UntrustedVMUtils::MitigationStatus::NOT_VULNERABLE;
  if (processor_mitigation_status != "Mitigation: PTE Inversion")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;

  const base::StringPiece& vmx_mitigation_status = l1tf_statuses[1];
  if (vmx_mitigation_status == "VMX: vulnerable")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;
  if (vmx_mitigation_status == "VMX: conditional cache flushes")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;
  if (vmx_mitigation_status != "VMX: cache flushes")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;

  const base::StringPiece& smt_mitigation_status = l1tf_statuses[2];
  if (smt_mitigation_status == "SMT vulnerable")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE_DUE_TO_SMT_ENABLED;
  if (smt_mitigation_status != "SMT disabled")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;

  return UntrustedVMUtils::MitigationStatus::NOT_VULNERABLE;
}

// Returns the MDS mitigation status of the host it's run on.
UntrustedVMUtils::MitigationStatus GetMDSMitigationStatus(
    const base::FilePath& mds_status_path) {
  std::string mds_status;
  if (!base::ReadFileToString(mds_status_path, &mds_status)) {
    LOG(ERROR) << "Failed to read L1TF status";
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;
  }

  std::vector<base::StringPiece> mds_statuses = base::SplitStringPiece(
      mds_status, ",;", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  // The sysfs file should always return up to 2 statuses and no more.
  DCHECK_LE(mds_statuses.size(), 2);

  const base::StringPiece& processor_mitigation_status = mds_statuses[0];
  if (processor_mitigation_status == "Not affected")
    return UntrustedVMUtils::MitigationStatus::NOT_VULNERABLE;
  if (processor_mitigation_status.find("Vulnerable") != base::StringPiece::npos)
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;
  if (processor_mitigation_status != "Mitigation: Clear CPU buffers")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;

  const base::StringPiece& smt_mitigation_status = mds_statuses[1];
  if (smt_mitigation_status == "SMT vulnerable")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE_DUE_TO_SMT_ENABLED;
  if (smt_mitigation_status == "SMT mitigated")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE_DUE_TO_SMT_ENABLED;
  if (smt_mitigation_status == "SMT Host state unknown")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE_DUE_TO_SMT_ENABLED;
  if (smt_mitigation_status != "SMT disabled")
    return UntrustedVMUtils::MitigationStatus::VULNERABLE;

  return UntrustedVMUtils::MitigationStatus::NOT_VULNERABLE;
}

}  // namespace

UntrustedVMUtils::UntrustedVMUtils(
    KernelVersionAndMajorRevision min_needed_version,
    const base::FilePath& l1tf_status_path,
    const base::FilePath& mds_status_path)
    : min_needed_version_(min_needed_version),
      l1tf_status_path_(l1tf_status_path),
      mds_status_path_(mds_status_path) {
  DCHECK(!l1tf_status_path.empty());
  DCHECK(!mds_status_path.empty());
}

UntrustedVMUtils::MitigationStatus
UntrustedVMUtils::CheckUntrustedVMMitigationStatus() {
  if (!host_kernel_version_) {
    host_kernel_version_ = GetKernelVersion();
    if (!host_kernel_version_) {
      LOG(ERROR) << "Failed to get kernel version";
      return MitigationStatus::VULNERABLE;
    }
  }

  if (host_kernel_version_.value() < min_needed_version_)
    return MitigationStatus::VULNERABLE;

  MitigationStatus status = GetL1TFMitigationStatus(l1tf_status_path_);
  if (status != MitigationStatus::NOT_VULNERABLE)
    return status;

  return GetMDSMitigationStatus(mds_status_path_);
}

bool UntrustedVMUtils::DisableSMT() {
  // TODO(abhishekbh): Disable SMT.
  return true;
}

void UntrustedVMUtils::SetKernelVersionForTesting(
    KernelVersionAndMajorRevision host_kernel_version) {
  host_kernel_version_ = host_kernel_version;
}

}  // namespace concierge
}  // namespace vm_tools
