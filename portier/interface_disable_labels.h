// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_INTERFACE_DISABLE_LABELS_H_
#define PORTIER_INTERFACE_DISABLE_LABELS_H_

#include <bitset>

namespace portier {

// The InterfaceDisableLabels is used to simplify the process of
// enabling and disabling interfaces.  There are several reasons for
// disabling an interface, most of them are independant or each other,
// and the interface should not be reenabled until all reasons have
// been changed.
//
// To use this class, have the interface inherit from this class and
// implement the callbacks OnEnabled() and OnDisable().
//
// Reasons for being disabled can be classified as "hard" or "soft".
//  - Soft - Soft reasons are ones that can occur from normal operation
//    of the ND proxy, but should not prevent external processes from
//    being able to reenabled them on command.
//  - Hard - Hard reasons can occur from both normal operator and
//    exceptional circumstances during the ND proxy's runtime.  These
//    circumstances must be cleared before allowing external processes
//    from being able to reenable the interface.
//
// Reasons for disabling an interface:
//  - Software Disabled (soft) - Another process or a system user has
//    requested that an interface to be disabled.
//  - ND Loop Detected (soft) - One of conditions for loop prevention
//    have occured on an interface.  The interface must be disabled
//    temporarily before attempting to reuse it.  This could have been
//    due to a configuration issue or by a test packet, as such it is
//    treated as non-external.
//  - Link Down  (hard) - The network interface has been set into a
//    DOWN state by the OS or by another processes.  Interface must be
//    brought up before allowing the interface to be used again.
//  - Not Group Member (hard) - If an interface is not a member of a
//    proxy groupe, then it should not be allowed to be enabled.
//
// The interface are enabled and disabled using a callback.  The
// callbacks are expected to take the name of the interface as its
// only parameter and return true if it successfully trigger a state
// change.
//
// When an interface receives its first reason for being disabled,
// the disable callback is called.  Any subsequent reasons are
// tracked, but do not cause an other call to disable.
//
class InterfaceDisableLabels {
 public:
  // Currently allocated number of flags.  This number is much larger
  // then needed to allow for extensions.
  static constexpr size_t kFlagCount = 32;
  // Reason flags, used internally for tracking which reasons have
  // been triggered on an interface.
  using Flags = std::bitset<kFlagCount>;

  InterfaceDisableLabels() = default;
  virtual ~InterfaceDisableLabels() = default;

  // Enables an interface only if the interface has no reasons to be
  // disabled.  Returns true if the OnEnable was called().
  bool TryEnable();

  // Clears all of the soft labels currently tracked.  Optionally,
  // |use_callback| can be set to true, in which the OnEnable()
  // callback will be called if there are no hard reasons.  Returns
  // true only if OnEnabled() was called.  If |use_callback| is
  // false, then the function will always return false.
  bool ClearSoftLabels(bool use_callback = false);

  // Clears all labels currently tracked.  Optionally, |use_callback|
  // can be set to true, in which the OnEnable() callback will be
  // called after clearing the labels.
  void ClearAllLabels(bool use_callback = false);

  // For all of the following methods:
  //  - The `Mark*' will label the interface as having reason *. The
  //    `OnDisabled()' callback will be called if there were no
  //    previous marked labels.  They return true if the callback
  //    was called.
  //  - The `Clear*' will clear the label.  The `OnEnabled()' callback
  //    will be called if clearing the label removed the last label.
  //    They return true if the callback was called.
  //  - The `IsMarked*' return true if the specified label is marked.

  bool MarkSoftwareDisabled(bool use_callback = true);
  bool ClearSoftwareDisabled(bool use_callback = true);
  bool IsMarkedSoftwareDisabled() const;

  bool MarkLoopDetected();
  bool ClearLoopDetected();
  bool IsMarkedLoopDetected() const;

  bool MarkLinkDown();
  bool ClearLinkDown();
  bool IsMarkedLinkDown() const;

  bool MarkGroupless(bool use_callback = true);
  bool ClearGroupless();
  bool IsMarkedGroupless();

 protected:
  // Callback for enabling a interface.  Called when all labels are
  // cleared.
  virtual void OnEnabled() = 0;
  // Callback for disabling a interface.  Called when first label is
  // marked.
  virtual void OnDisabled() = 0;

 private:
  bool SetFlag(size_t flag_pos, bool use_callback);
  bool ClearFlag(size_t flag_pos, bool use_callback);

  Flags reason_flags_;
};

}  // namespace portier

#endif  // PORTIER_INTERFACE_DISABLE_LABELS_H_
