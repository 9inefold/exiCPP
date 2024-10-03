//===- Content.cpp --------------------------------------------------===//
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

#include <BinaryBuffer.hpp>
#include <cstdio>

using namespace exi;

static std::size_t readFromFP(void* buf, std::size_t count, void* stream) {
  auto* const fp = static_cast<std::FILE*>(stream);
  const auto ret = std::fread(buf, sizeof(Char), count, fp);
  return ret;
}

static std::size_t writeToFP(void* buf, std::size_t count, void* stream) {
  auto* const fp = static_cast<std::FILE*>(stream);
  const auto ret = std::fwrite(buf, sizeof(Char), count, fp);
  return ret;
}

Error IBinaryBuffer::readFile(StrRef name) {
  this->destroyStream();
  auto* fp = std::fopen(name.data(), "rb");
  if (!fp)
    return Error::From("Unable to open file to read.");
  
  this->stream_type = StreamType::RFile;
  this->ioStrm.readWriteToStream = &readFromFP;
  this->ioStrm.stream = fp;

  return Error::Ok();
}

Error IBinaryBuffer::writeFile(StrRef name) {
  this->destroyStream();
  auto* fp = std::fopen(name.data(), "wb");
  if (!fp)
    return Error::From("Unable to open file to write.");
  
  this->stream_type = StreamType::WFile;
  this->ioStrm.readWriteToStream = &writeToFP;
  this->ioStrm.stream = fp;

  return Error::Ok();
}

//////////////////////////////////////////////////////////////////////////

void IBinaryBuffer::setInternal(Char* data, std::size_t len) {
  if EXICPP_UNLIKELY(this->isSameBuffer(data, len))
    return;
  CBinaryBuffer::buf = data;
  CBinaryBuffer::bufLen = len;
  CBinaryBuffer::bufContent = 0;
}

void IBinaryBuffer::destroyStream() {
  auto& stream = this->ioStrm;
  switch (this->stream_type) {
   case StreamType::RFile:
   case StreamType::WFile: {
    auto* const fp = static_cast<std::FILE*>(stream.stream);
    std::fclose(fp);
    break;
   }
  }
  stream = exip::IOStream{};
}
