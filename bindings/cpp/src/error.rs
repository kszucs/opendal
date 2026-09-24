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
//! [`opendal::ErrorKind`]. [`FfiError`] instead encodes the kind and whether the
//! failure is temporary in front of the message, and `src/error.cpp` decodes them
//! back into a typed `opendal::Error`.

use std::fmt;

use opendal as od;

/// Unit separator. OpenDAL error messages are human-readable text and never
/// contain control characters, so it cannot collide with the payload.
const FIELD_SEPARATOR: char = '\x1f';

/// Stable numeric codes for [`od::ErrorKind`], mirrored by `opendal::ErrorKind`
/// in `include/error.hpp`. Never renumber an existing entry, only append.
#[derive(Clone, Copy, Debug)]
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
#[derive(Debug)]
pub struct FfiError {
    kind: ErrorKindCode,
    temporary: bool,
    message: String,
}

impl FfiError {
    /// An error raised by the binding itself (a bad seek direction, an integer
    /// conversion that cannot fit), which retrying will not resolve.
    pub fn other(message: impl Into<String>) -> Self {
        Self {
            kind: ErrorKindCode::Unexpected,
            temporary: false,
            message: message.into(),
        }
    }
}

impl fmt::Display for FfiError {
    /// Emits `<kind code><US><0|1 temporary><US><message>`, decoded by
    /// `opendal::details::RethrowFfiError`. The message is last, so it may contain
    /// anything, including further separators.
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{kind}{sep}{temporary}{sep}{message}",
            sep = FIELD_SEPARATOR,
            kind = self.kind as u8,
            temporary = u8::from(self.temporary),
            message = self.message,
        )
    }
}

impl std::error::Error for FfiError {}

impl From<od::Error> for FfiError {
    fn from(err: od::Error) -> Self {
        Self {
            kind: err.kind().into(),
            temporary: err.is_temporary(),
            // The full Display form rather than `err.message()`, which drops the
            // operation and context that make the error diagnosable.
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
pub type Result<T> = std::result::Result<T, FfiError>;
