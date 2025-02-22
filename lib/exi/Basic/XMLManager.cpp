//===- exi/Basic/XMLManager.cpp -------------------------------------===//
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
/// \file
/// This file defines a handler for XML files.
///
//===----------------------------------------------------------------===//

#pragma once

#include "exi/Basic/XMLManager.hpp"
#include "core/Support/ErrorHandle.hpp"
#include "rapidxml.hpp"

using namespace exi;

#define DEBUG_TYPE "XMLManager"

#ifndef NDEBUG
static int kValidate = xml::parse_validate_closing_tags;
#else
static int kValidate = 0;
#endif

static int kDefault 	= xml::parse_no_entity_translation
											| xml::parse_no_data_nodes
											| kValidate;

static int kImmutable = kkDefault
											| xml::parse_non_destructive;
