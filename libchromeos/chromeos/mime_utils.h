// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_MIME_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_MIME_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <chromeos/chromeos_export.h>

namespace chromeos {
namespace mime {

namespace types {
// Main MIME type categories
CHROMEOS_EXPORT extern const char kApplication[];        // application
CHROMEOS_EXPORT extern const char kAudio[];              // audio
CHROMEOS_EXPORT extern const char kImage[];              // image
CHROMEOS_EXPORT extern const char kMessage[];            // message
CHROMEOS_EXPORT extern const char kMultipart[];          // multipart
CHROMEOS_EXPORT extern const char kText[];               // test
CHROMEOS_EXPORT extern const char kVideo[];              // video
}  // namespace types

namespace parameters {
// Common MIME parameters
CHROMEOS_EXPORT extern const char kCharset[];            // charset=...
}  // namespace parameters

namespace image {
// Common image MIME types
CHROMEOS_EXPORT extern const char kJpeg[];               // image/jpeg
CHROMEOS_EXPORT extern const char kPng[];                // image/png
CHROMEOS_EXPORT extern const char kBmp[];                // image/bmp
CHROMEOS_EXPORT extern const char kTiff[];               // image/tiff
CHROMEOS_EXPORT extern const char kGif[];                // image/gif
}  // namespace image

namespace text {
// Common text MIME types
CHROMEOS_EXPORT extern const char kPlain[];              // text/plain
CHROMEOS_EXPORT extern const char kHtml[];               // text/html
CHROMEOS_EXPORT extern const char kXml[];                // text/xml
}  // namespace text

namespace application {
// Common application MIME types
// application/octet-stream
CHROMEOS_EXPORT extern const char kOctet_stream[];
// application/json
CHROMEOS_EXPORT extern const char kJson[];
// application/x-www-form-urlencoded
CHROMEOS_EXPORT extern const char kWwwFormUrlEncoded[];
// application/x-protobuf
CHROMEOS_EXPORT extern const char kProtobuf[];
}  // namespace application

namespace multipart {
// Common multipart MIME types
// multipart/form-data
CHROMEOS_EXPORT extern const char kFormData[];
// multipart/mixed
CHROMEOS_EXPORT extern const char kMixed[];
}  // namespace multipart

using Parameters = std::vector<std::pair<std::string, std::string>>;

// Combine a MIME type, subtype and parameters into a MIME string.
// e.g. Combine("text", "plain", {{"charset", "utf-8"}}) will give:
//      "text/plain; charset=utf-8"
CHROMEOS_EXPORT std::string Combine(
    const std::string& type,
    const std::string& subtype,
    const Parameters& parameters = {}) WARN_UNUSED_RESULT;

// Splits a MIME string into type and subtype.
// "text/plain;charset=utf-8" => ("text", "plain")
CHROMEOS_EXPORT bool Split(const std::string& mime_string,
                           std::string* type,
                           std::string* subtype);

// Splits a MIME string into type, subtype, and parameters.
// "text/plain;charset=utf-8" => ("text", "plain", {{"charset","utf-8"}})
CHROMEOS_EXPORT bool Split(const std::string& mime_string,
                           std::string* type,
                           std::string* subtype,
                           Parameters* parameters);

// Returns the MIME type from MIME string.
// "text/plain;charset=utf-8" => "text"
CHROMEOS_EXPORT std::string GetType(const std::string& mime_string);

// Returns the MIME sub-type from MIME string.
// "text/plain;charset=utf-8" => "plain"
CHROMEOS_EXPORT std::string GetSubtype(const std::string& mime_string);

// Returns the MIME parameters from MIME string.
// "text/plain;charset=utf-8" => {{"charset","utf-8"}}
CHROMEOS_EXPORT Parameters GetParameters(const std::string& mime_string);

// Removes parameters from a MIME string
// "text/plain;charset=utf-8" => "text/plain"
CHROMEOS_EXPORT std::string RemoveParameters(
    const std::string& mime_string) WARN_UNUSED_RESULT;

// Appends a parameter to a MIME string.
// "text/plain" => "text/plain; charset=utf-8"
CHROMEOS_EXPORT std::string AppendParameter(
    const std::string& mime_string,
    const std::string& paramName,
    const std::string& paramValue) WARN_UNUSED_RESULT;

// Returns the value of a parameter on a MIME string (empty string if missing).
// ("text/plain;charset=utf-8","charset") => "utf-8"
CHROMEOS_EXPORT std::string GetParameterValue(const std::string& mime_string,
                                              const std::string& paramName);

}  // namespace mime
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_MIME_UTILS_H_
