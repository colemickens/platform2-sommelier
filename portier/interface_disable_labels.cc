// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/interface_disable_labels.h"

#include <base/logging.h>

namespace portier {

using Flags = InterfaceDisableLabels::Flags;

namespace {

// Each reason for being disabled has its own bit flag.
// - Soft reasons - bits 0 to 15
// - Hard reasons - bits 16 to 32
constexpr Flags kSoftFlagMask = Flags(0x0000ffffu);
constexpr Flags kHardFlagMask = Flags(0xffff0000u);

// Soft reason flags.
constexpr size_t kFlagSoftwareDisablePos = 0;
constexpr size_t kFlagLoopDetectedPos = 1;

// Hard reason flags.
constexpr size_t kFlagLinkDownPos = 16;
constexpr size_t kFlagGrouplessPos = 17;

// Returns true if any of the provided flags are for hard reasons.
bool HasHardReason(const Flags& flags) {
  return (flags & kHardFlagMask).any();
}

bool HasReason(const Flags& flags) {
  return flags.any();
}

}  // namespace

bool InterfaceDisableLabels::TryEnable() {
  if (HasReason(reason_flags_)) {
    return false;
  }
  OnEnabled();
  return true;
}

bool InterfaceDisableLabels::ClearSoftLabels(bool use_callback) {
  reason_flags_ &= ~kSoftFlagMask;
  if (!HasHardReason(reason_flags_) && use_callback) {
    OnEnabled();
    return true;
  }
  return false;
}

void InterfaceDisableLabels::ClearAllLabels(bool use_callback) {
  reason_flags_.reset();
  if (use_callback) {
    OnEnabled();
  }
}

bool InterfaceDisableLabels::MarkSoftwareDisabled(bool use_callback) {
  return SetFlag(kFlagSoftwareDisablePos, use_callback);
}
bool InterfaceDisableLabels::ClearSoftwareDisabled(bool use_callback) {
  return ClearFlag(kFlagSoftwareDisablePos, use_callback);
}
bool InterfaceDisableLabels::IsMarkedSoftwareDisabled() const {
  return reason_flags_.test(kFlagSoftwareDisablePos);
}

bool InterfaceDisableLabels::MarkLoopDetected() {
  return SetFlag(kFlagLoopDetectedPos, true);
}
bool InterfaceDisableLabels::ClearLoopDetected() {
  return ClearFlag(kFlagLoopDetectedPos, true);
}
bool InterfaceDisableLabels::IsMarkedLoopDetected() const {
  return reason_flags_.test(kFlagLoopDetectedPos);
}

bool InterfaceDisableLabels::MarkLinkDown() {
  return SetFlag(kFlagLinkDownPos, true);
}
bool InterfaceDisableLabels::ClearLinkDown() {
  return ClearFlag(kFlagLinkDownPos, true);
}
bool InterfaceDisableLabels::IsMarkedLinkDown() const {
  return reason_flags_.test(kFlagLinkDownPos);
}

bool InterfaceDisableLabels::MarkGroupless(bool use_callback) {
  return SetFlag(kFlagGrouplessPos, use_callback);
}
bool InterfaceDisableLabels::ClearGroupless() {
  return ClearFlag(kFlagGrouplessPos, true);
}
bool InterfaceDisableLabels::IsMarkedGroupless() {
  return reason_flags_.test(kFlagGrouplessPos);
}

bool InterfaceDisableLabels::SetFlag(size_t flag_pos, bool use_callback) {
  DCHECK_LT(flag_pos, reason_flags_.size());
  const Flags old_flags = reason_flags_;
  reason_flags_.set(flag_pos);
  if (!HasReason(old_flags) && use_callback) {
    OnDisabled();
    return true;
  }
  return false;
}

bool InterfaceDisableLabels::ClearFlag(size_t flag_pos, bool use_callback) {
  DCHECK_LT(flag_pos, reason_flags_.size());
  const Flags old_flags = reason_flags_;
  reason_flags_.reset(flag_pos);
  if (HasReason(old_flags) && !HasReason(reason_flags_) && use_callback) {
    OnEnabled();
    return true;
  }
  return false;
}

}  // namespace portier
