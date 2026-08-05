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

/// Internal header, deliberately not under include/: it names `rust::Error`, and
/// the generated cxxbridge include directory is only on the public include path
/// when the async feature is enabled. Keeping cxx types out of the installed
/// headers means consumers never need that directory.

#include <utility>

#include "error.hpp"
#include "rust/cxx.h"

namespace opendal::details {

/**
 * @brief Decode a cxx `rust::Error` raised by a bridge call and rethrow it as a
 * typed opendal::Error.
 *
 * cxx gives a C++ exception carrying only the Rust error's Display string, so
 * `src/error.rs` encodes the kind and temporary flag into that string and this
 * function decodes them back out. An error whose payload is not in that format
 * (one raised below the bridge, say) becomes ErrorKind::Unexpected with the
 * original text preserved.
 */
[[noreturn]] void RethrowFfiError(const ::rust::Error &e);

/**
 * @brief Invoke a bridge call, translating any `rust::Error` it throws.
 *
 * Every call into `ffi::` from this binding's C++ layer goes through here, so
 * that no untyped `rust::Error` escapes the public API.
 */
template <typename F>
decltype(auto) Call(F &&f) {
  try {
    return std::forward<F>(f)();
  } catch (const ::rust::Error &e) {
    RethrowFfiError(e);
  }
}

}  // namespace opendal::details
