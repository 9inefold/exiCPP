//===- exi/Encode/XMLSerializer.hpp ----------------------------------===//
//
// Copyright (C) 2025 Ninefold
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
/// This file implements the interface used to encode XML as EXI.
///
//===----------------------------------------------------------------===//

#pragma once

#include <core/Common/Option.hpp>
#include <exi/Basic/ErrorCodes.hpp>
#include <exi/Basic/XML.hpp>
#include <exi/Encode/Serializer.hpp>
#include <rapidxml.hpp>

namespace exi {

struct XMLSerializerOpts {
  /// If CDATA blocks should be preserved.
  bool PreserveCDATA = true;
};

class XMLSerializer final : public Serializer, public XMLSerializerOpts {
  /// The document this serializer is bound to.
  XMLDocument* Doc = nullptr;
public:
  XMLSerializer(XMLDocument& Doc) : XMLSerializer(&Doc) {}
  XMLSerializer(XMLDocument* Doc) : Doc(Doc) {}
  ExiError exec(BodyEncoder* BE) override;
private:
  void anchor() override;
};

class OwningXMLSerializer final : public Serializer, public XMLSerializerOpts {
  XMLDocument Doc;
public:
  OwningXMLSerializer() : Doc() {}
  OwningXMLSerializer(Option<xml::XMLBumpAllocator&> A) : Doc(A) {}
  ExiError exec(BodyEncoder* BE) override;
private:
  void anchor() override;
};

} // namespace exi
