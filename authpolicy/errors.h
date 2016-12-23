// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_ERRORS_H_
#define AUTHPOLICY_ERRORS_H_

namespace errors {

// Brillo error domain, see brillo::Error.
extern const char kDomain[];

// Brillo error codes, see brillo::Error.
extern const char kCreateDirFailed[];        // Failed to create a directory.
extern const char kDownloadGpoFailed[];      // Failed to download GPO.
extern const char kInvalidGpoPaths[];        // Bad GPO paths.
extern const char kKInitFailed[];            // Unknown kinit error.
extern const char kKInitBadUserName[];       // Bad user name.
extern const char kKInitBadPassword[];       // Bad password.
extern const char kKInitPasswordExpired[];   // Password expired.
extern const char kKInitCannotResolve[];     // Cannot resolve KDC realm.
extern const char kNetAdsGpoListFailed[];    // Getting GPO list failed.
extern const char kNetAdsInfoFailed[];       // Getting DC name failed.
extern const char kNetAdsJoinFailed[];       // Joining machine to AD failed.
extern const char kNetAdsSearchFailed[];     // Getting account id failed.
extern const char kNotLoggedIn[];            // Can't fetch GPOs, not logged in.
extern const char kParseGpoFailed[];         // Can't parse gpo list result.
extern const char kParseGpoPathFailed[];     // Failed to parse GPO filesyspath.
extern const char kParseNetAdsInfoFailed[];  // Can't parse DC name info.
extern const char kParseNetAdsSearchFailed[];  // Can't parse account id info.
extern const char kParseUPNFailed[];         // Can't parse user principal name.
extern const char kPregFileNotFound[];       // Preg file path does not exist.
extern const char kPregParseError[];         // Failed to parse a preg file.
extern const char kPregReadError[];          // Failed to load a preg file.
extern const char kPregTooBig[];             // Preg file was too big.
extern const char kSetPermissionsFailed[];   // Failed to set file permissions.
extern const char kSmbClientFailed[];        // Downloading GPOs failed.
extern const char kStorePolicyFailed[];      // Can't send to Session Manager.
extern const char kWriteConfigFailed[];      // Failed to write config fle.
extern const char kWriteKrb5ConfFailed[];    // Failed to write krb5 conf file.
extern const char kWriteSmbConfFailed[];     // Failed to write samba conf file.

}  // namespace errors

#endif  // AUTHPOLICY_ERRORS_H_
