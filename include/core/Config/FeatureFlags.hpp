//===- Config/FeatureFlags.inc --------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Licensed under the Apache License, Version 2.0 (the "License");
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
///
/// Macros to check if a given feature has been implemented.
///
//===----------------------------------------------------------------===//

#pragma once

#include <Config/Config.inc>

#define EXI_CUSTOM_STRREF 1
// TODO: Implement arbitrary precision scalars (APInt, APFloat)
#define EXI_HAS_AP_SCALARS 0
#define EXI_HAS_CRASHRECOVERYCONTEXT 0
// TODO: Implement DenseSet
#define EXI_HAS_DENSE_MAP 0
#define EXI_HAS_DENSE_SET EXI_HAS_DENSE_MAP
// TODO: Implement file streams (will be useful later...)
#define EXI_HAS_RAW_FILE_STREAMS 1
#define EXI_HAS_DBG_IMPL 1
// TODO: Implement system headers
#define EXI_HAS_SYS_IMPL 0
