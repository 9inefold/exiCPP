//===- Support/AutoConvert.hpp --------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//
//
// This file contains functions used for auto conversion between
// ASCII/EBCDIC codepages specific to z/OS.
//
//===----------------------------------------------------------------===//

#pragma once

#ifdef __MVS__

#include <_Ccsid.h>
#ifdef __cplusplus
#include <Support/ErrorOr.hpp>
#include <system_error>
#endif /* __cplusplus */

#define CCSID_IBM_1047 1047
#define CCSID_UTF_8 1208
#define CCSID_ISO8859_1 819

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
int enablezOSAutoConversion(int FD);
int disablezOSAutoConversion(int FD);
int restorezOSStdHandleAutoConversion(int FD);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus
namespace exi {

/** \brief Disable the z/OS enhanced ASCII auto-conversion for the file
 * descriptor.
 */
std::error_code disablezOSAutoConversion(int FD);

/** \brief Query the z/OS enhanced ASCII auto-conversion status of a file
 * descriptor and force the conversion if the file is not tagged with a
 * codepage.
 */
std::error_code enablezOSAutoConversion(int FD);

/** Restore the z/OS enhanced ASCII auto-conversion for the std handle. */
std::error_code restorezOSStdHandleAutoConversion(int FD);

/** \brief Set the tag information for a file descriptor. */
std::error_code setzOSFileTag(int FD, int CCSID, bool Text);

// Get the the tag ccsid for a file name or a file descriptor.
ErrorOr<__ccsid_t> getzOSFileTag(const char *FileName, const int FD = -1);

// Query the file tag to determine if it needs conversion to UTF-8 codepage.
ErrorOr<bool> needzOSConversion(const char *FileName, const int FD = -1);

} // namespace exi
#endif // __cplusplus

#endif /* __MVS__ */
