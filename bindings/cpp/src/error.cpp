/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "ffi_error.hpp"

#include <charconv>
#include <string>
#include <string_view>

namespace opendal {

namespace {

/// Must match `FIELD_SEPARATOR`, `PAYLOAD_PREFIX` and `FORMAT_VERSION` in
/// src/error.rs. See that file for the payload layout.
constexpr char kFieldSeparator = '\x1f';
constexpr std::string_view kPayloadPrefix = "opendal-ffi";
constexpr int kFormatVersion = 1;

/// Parse a whole string_view as a non-negative integer, rejecting trailing junk.
bool ParseInt(std::string_view text, int &out) {
  const auto *end = text.data() + text.size();
  auto result = std::from_chars(text.data(), end, out);
  return result.ec == std::errc{} && result.ptr == end;
}

/// Split off the next separator-delimited field, advancing `rest`. Returns false
/// when no separator remains, meaning the payload is truncated.
bool NextField(std::string_view &rest, std::string_view &field) {
  auto pos = rest.find(kFieldSeparator);
  if (pos == std::string_view::npos) {
    return false;
  }
  field = rest.substr(0, pos);
  rest.remove_prefix(pos + 1);
  return true;
}

bool IsKnownKind(int code) {
  return code >= static_cast<int>(ErrorKind::Unexpected) &&
         code <= static_cast<int>(ErrorKind::RangeNotSatisfied);
}

}  // namespace

std::string_view ToStringView(ErrorKind kind) noexcept {
  switch (kind) {
    case ErrorKind::Unexpected:
      return "Unexpected";
    case ErrorKind::Unsupported:
      return "Unsupported";
    case ErrorKind::ConfigInvalid:
      return "ConfigInvalid";
    case ErrorKind::NotFound:
      return "NotFound";
    case ErrorKind::PermissionDenied:
      return "PermissionDenied";
    case ErrorKind::IsADirectory:
      return "IsADirectory";
    case ErrorKind::NotADirectory:
      return "NotADirectory";
    case ErrorKind::AlreadyExists:
      return "AlreadyExists";
    case ErrorKind::RateLimited:
      return "RateLimited";
    case ErrorKind::IsSameFile:
      return "IsSameFile";
    case ErrorKind::ConditionNotMatch:
      return "ConditionNotMatch";
    case ErrorKind::RangeNotSatisfied:
      return "RangeNotSatisfied";
  }
  return "Unknown";
}

Error::Error(ErrorKind kind, bool temporary, std::string message)
    : std::runtime_error(std::string("opendal: ")
                             .append(ToStringView(kind))
                             .append(temporary ? " (temporary): " : ": ")
                             .append(message)),
      kind_(kind),
      temporary_(temporary),
      message_(std::move(message)) {}

namespace details {

[[noreturn]] void RethrowFfiError(const ::rust::Error &e) {
  std::string_view payload{e.what()};

  // Anything not carrying our prefix was raised below the bridge and has no
  // kind to recover; surface it verbatim rather than guessing.
  const auto prefix_length = kPayloadPrefix.size() + 1;
  if (payload.size() < prefix_length ||
      payload.substr(0, kPayloadPrefix.size()) != kPayloadPrefix ||
      payload[kPayloadPrefix.size()] != kFieldSeparator) {
    throw Error(ErrorKind::Unexpected, /* temporary */ false,
                std::string(payload));
  }
  payload.remove_prefix(prefix_length);

  std::string_view version_field;
  std::string_view kind_field;
  std::string_view temporary_field;
  int version = 0;
  int kind_code = 0;
  int temporary = 0;

  // A payload we cannot fully decode is reported with its raw text instead of a
  // partially-trusted kind: a future format version must not be silently
  // reinterpreted under this version's layout.
  if (!NextField(payload, version_field) || !ParseInt(version_field, version) ||
      version != kFormatVersion || !NextField(payload, kind_field) ||
      !ParseInt(kind_field, kind_code) || !IsKnownKind(kind_code) ||
      !NextField(payload, temporary_field) ||
      !ParseInt(temporary_field, temporary) || temporary > 1) {
    throw Error(ErrorKind::Unexpected, /* temporary */ false,
                std::string(e.what()));
  }

  // Whatever remains is the message, which may itself contain separators.
  throw Error(static_cast<ErrorKind>(kind_code), temporary == 1,
              std::string(payload));
}

}  // namespace details

}  // namespace opendal
