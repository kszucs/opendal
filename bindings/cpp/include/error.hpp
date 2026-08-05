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

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace opendal {

/**
 * @brief Mirror of Rust's `opendal::ErrorKind`, so C++ callers can react to a
 * failure's category instead of matching on message text.
 *
 * @note The numeric values are part of the FFI wire format and must stay in
 * sync with `ErrorKindCode` in `src/error.rs`. Never renumber an existing
 * entry, only append.
 */
enum class ErrorKind : int {
  /// Something went wrong that OpenDAL cannot categorise. Also the value a
  /// kind added by a newer core degrades to, and the kind used for failures
  /// originating inside the binding itself.
  Unexpected = 1,
  /// The underlying service does not support this operation.
  Unsupported = 2,
  /// The configuration given for this service is invalid.
  ConfigInvalid = 3,
  /// The given path was not found.
  NotFound = 4,
  /// The credentials do not permit this operation on this path.
  PermissionDenied = 5,
  /// The given path is a directory.
  IsADirectory = 6,
  /// The given path is not a directory.
  NotADirectory = 7,
  /// The given path already exists.
  AlreadyExists = 8,
  /// The service is asking us to slow down.
  RateLimited = 9,
  /// The source and destination paths are the same.
  IsSameFile = 10,
  /// A conditional request's precondition was not met.
  ConditionNotMatch = 11,
  /// The requested byte range cannot be satisfied.
  RangeNotSatisfied = 12,
};

/**
 * @brief Human-readable name of a kind, e.g. "NotFound". Returns "Unknown" for
 * a value this build does not recognise.
 */
std::string_view ToStringView(ErrorKind kind) noexcept;

/**
 * @class Error
 * @brief The exception every failing OpenDAL C++ API throws.
 *
 * Derives from `std::runtime_error` so that callers who only catch
 * `std::exception` keep working, while callers who care about the category can
 * inspect Kind() and IsTemporary().
 */
class Error : public std::runtime_error {
 public:
  Error(ErrorKind kind, bool temporary, std::string message);

  /// The failure's category.
  ErrorKind Kind() const noexcept { return kind_; }

  /**
   * @brief Whether retrying the identical operation could plausibly succeed.
   *
   * True for transient conditions such as RateLimited or a network blip. Errors
   * raised by the binding itself are always permanent.
   */
  bool IsTemporary() const noexcept { return temporary_; }

  /// The underlying message, without the kind prefix that what() adds.
  const std::string &Message() const noexcept { return message_; }

 private:
  ErrorKind kind_;
  bool temporary_;
  std::string message_;
};

}  // namespace opendal
