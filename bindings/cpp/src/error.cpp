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

Error::Error(ErrorKind kind, bool temporary, const std::string &message)
    : std::runtime_error(message), kind_(kind), temporary_(temporary) {}

namespace details {

[[noreturn]] void RethrowFfiError(const ::rust::Error &e) {
  /// Must match `FIELD_SEPARATOR` and the layout written by `FfiError`'s
  /// Display in src/error.rs: `<kind code><US><0|1 temporary><US><message>`.
  constexpr char kFieldSeparator = '\x1f';
  const std::string_view payload{e.what()};

  const auto kind_end = payload.find(kFieldSeparator);
  const auto temporary_end = kind_end == std::string_view::npos
                                 ? std::string_view::npos
                                 : payload.find(kFieldSeparator, kind_end + 1);
  int kind_code = 0;
  const bool decoded =
      temporary_end == kind_end + 2 &&
      std::from_chars(payload.data(), payload.data() + kind_end, kind_code)
              .ptr == payload.data() + kind_end &&
      kind_code >= static_cast<int>(ErrorKind::Unexpected) &&
      kind_code <= static_cast<int>(ErrorKind::RangeNotSatisfied);

  // Anything we cannot decode was not produced by `FfiError`; surface it
  // verbatim rather than guessing at a kind.
  if (!decoded) {
    throw Error(ErrorKind::Unexpected, /* temporary */ false,
                std::string(payload));
  }

  throw Error(static_cast<ErrorKind>(kind_code), payload[kind_end + 1] == '1',
              std::string(payload.substr(temporary_end + 1)));
}

}  // namespace details

}  // namespace opendal
