// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

//! Error type carried across the cxx FFI boundary.
//!
//! cxx maps a `Result<T>` error into a C++ exception that carries nothing but the
//! error's `Display` string, so returning `anyhow::Error` here would discard
//! [`opendal::ErrorKind`] — callers could only recover it by parsing OpenDAL's
//! human-readable error text, which is far too fragile to put under retry logic.
//!
//! [`FfiError`] instead encodes the pieces a caller needs to act on (the error
//! kind and whether the failure is temporary) into a delimiter-separated payload
//! that the C++ side decodes back into a typed `opendal::Error`. The encoder here
//! and the decoder in `src/error.cpp` are versioned together by `FORMAT_VERSION`.

use std::fmt;

use opendal as od;

/// Unit separator. Chosen because OpenDAL error messages are human-readable text
/// and never contain control characters, so it cannot collide with the payload.
pub const FIELD_SEPARATOR: char = '\x1f';

/// Payload prefix, used by the C++ side to tell an encoded error apart from a
/// plain string error that some other layer may have produced.
pub const PAYLOAD_PREFIX: &str = "opendal-ffi";

/// Bumped whenever the wire format below changes. The decoder rejects payloads
/// carrying any other version rather than guessing at their layout.
pub const FORMAT_VERSION: u32 = 1;

/// Stable numeric codes for [`od::ErrorKind`], mirrored by `opendal::ErrorKind`
/// in `include/opendal.hpp`. These are part of the FFI wire format: never
/// renumber an existing entry, only append.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum ErrorKindCode {
    Unexpected = 1,
    Unsupported = 2,
    ConfigInvalid = 3,
    NotFound = 4,
    PermissionDenied = 5,
    IsADirectory = 6,
    NotADirectory = 7,
    AlreadyExists = 8,
    RateLimited = 9,
    IsSameFile = 10,
    ConditionNotMatch = 11,
    RangeNotSatisfied = 12,
}

impl From<od::ErrorKind> for ErrorKindCode {
    fn from(kind: od::ErrorKind) -> Self {
        match kind {
            od::ErrorKind::Unexpected => Self::Unexpected,
            od::ErrorKind::Unsupported => Self::Unsupported,
            od::ErrorKind::ConfigInvalid => Self::ConfigInvalid,
            od::ErrorKind::NotFound => Self::NotFound,
            od::ErrorKind::PermissionDenied => Self::PermissionDenied,
            od::ErrorKind::IsADirectory => Self::IsADirectory,
            od::ErrorKind::NotADirectory => Self::NotADirectory,
            od::ErrorKind::AlreadyExists => Self::AlreadyExists,
            od::ErrorKind::RateLimited => Self::RateLimited,
            od::ErrorKind::IsSameFile => Self::IsSameFile,
            od::ErrorKind::ConditionNotMatch => Self::ConditionNotMatch,
            od::ErrorKind::RangeNotSatisfied => Self::RangeNotSatisfied,
            // `od::ErrorKind` is #[non_exhaustive]: a kind added upstream must
            // degrade to Unexpected rather than fail to compile here.
            _ => Self::Unexpected,
        }
    }
}

/// The error type returned by every `extern "Rust"` bridge function.
pub struct FfiError {
    kind: ErrorKindCode,
    /// Whether retrying the same operation could plausibly succeed.
    temporary: bool,
    message: String,
}

impl FfiError {
    /// An error originating in the binding itself rather than in OpenDAL. These
    /// are always permanent: they signal a misuse of the FFI API (a bad seek
    /// direction, an integer conversion that cannot fit) that retrying will not
    /// resolve.
    pub fn other(message: impl Into<String>) -> Self {
        Self {
            kind: ErrorKindCode::Unexpected,
            temporary: false,
            message: message.into(),
        }
    }
}

impl fmt::Display for FfiError {
    /// Emits the wire format decoded by `opendal::details::RethrowFfiError`:
    ///
    /// ```text
    /// opendal-ffi<US>1<US><kind code><US><0|1 temporary><US><message>
    /// ```
    ///
    /// The message is last so that it may itself contain anything at all,
    /// including further separators, without confusing the decoder.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{PAYLOAD_PREFIX}{sep}{FORMAT_VERSION}{sep}{kind}{sep}{temporary}{sep}{message}",
            sep = FIELD_SEPARATOR,
            kind = self.kind as u8,
            temporary = u8::from(self.temporary),
            message = self.message,
        )
    }
}

impl fmt::Debug for FfiError {
    /// Deliberately human-readable rather than the wire format: this is what
    /// shows up in Rust-side panics and test failures.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("FfiError")
            .field("kind", &self.kind)
            .field("temporary", &self.temporary)
            .field("message", &self.message)
            .finish()
    }
}

impl std::error::Error for FfiError {}

impl From<od::Error> for FfiError {
    /// The conversion that lets every bridge function keep using `?` on
    /// OpenDAL's own results unchanged.
    fn from(err: od::Error) -> Self {
        Self {
            kind: err.kind().into(),
            temporary: err.is_temporary(),
            // The full Display form, not `err.message()`: it carries the
            // operation and context that make the error diagnosable, and
            // `message()` alone drops them.
            message: err.to_string(),
        }
    }
}

impl From<std::num::TryFromIntError> for FfiError {
    fn from(err: std::num::TryFromIntError) -> Self {
        Self::other(err.to_string())
    }
}

/// Result alias used by the bridge, replacing `anyhow::Result`.
pub type Result<T, E = FfiError> = std::result::Result<T, E>;
