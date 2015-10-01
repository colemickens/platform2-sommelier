// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_LOCKED_SETTINGS_H_
#define FIDES_LOCKED_SETTINGS_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "fides/blob_ref.h"

namespace fides {

class SettingsDocument;

// LockedVersionComponent wraps signatures and meta data used in validating
// version stamps that are included within a LockedSettingsContainer. This
// enables the validation logic implemented in SourceDelegate subclasses to
// perform validation without having to know details about the binary encoding
// of the wrapped version component.
class LockedVersionComponent {
 public:
  virtual ~LockedVersionComponent() = default;

  // Returns the source identifier this version component belongs to.
  virtual std::string GetSourceId() const = 0;

 private:
  DISALLOW_ASSIGN(LockedVersionComponent);
};

// LockedSettingsContainer holds context used while decoding and validating the
// enclosed SettingsDocument. This interface provides access to signatures and
// meta data present in the protected container regardless of binary encoding of
// the container. Note that the actual validation logic is not part of this
// class. It is specific to the source that has produced the SettingsDocument
// enclosed within a LockedSettingsContainer and the validation logic is
// implemented in a suitable SourceDelegate subclass. For example, the source
// delegate might check whether there's a valid signature on the container,
// using a signing key configured for the source for signature verification.
class LockedSettingsContainer {
 public:
  virtual ~LockedSettingsContainer() = default;

  // Gets the protected data payload. If unavailable, returns an empty blob.
  virtual BlobRef GetData() const;

  // Get the protected vector clock components (along with applicable
  // signatures, meta data etc.) for validation against the source-specific
  // logic implemented in the corresponding SourceDelegate.
  //
  // Returns an empty vector in case the blob doesn't contain any locked version
  // components. Note that this implies the SettingsDocument returned by
  // DecodePayload will not have a version stamp that establishes causal
  // relation with other documents, so it'll only pass validation if the
  // settings keys it touches are not present in the system yet.
  virtual std::vector<const LockedVersionComponent*> GetVersionComponents()
      const;

  // Extract the enclosed SettingsDocument payload from the passed container.
  static std::unique_ptr<SettingsDocument> DecodePayload(
      std::unique_ptr<LockedSettingsContainer> container);

 private:
  // Decodes the payload, i.e. extracts the enclosed SettingsDocument. It is
  // guaranteed that this function only gets called exactly once and the |this|
  // object will get destroyed immediately afterwards.
  virtual std::unique_ptr<SettingsDocument> DecodePayloadInternal() = 0;

  DISALLOW_ASSIGN(LockedSettingsContainer);
};

}  // namespace fides

#endif  // FIDES_LOCKED_SETTINGS_H_
