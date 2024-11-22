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

#define PRINT_STATS 0

using namespace exi;

#if PRINT_STATS

#include <Debug/Format.hpp>
#include <fmt/chrono.h>

static std::size_t totalCount = 0;
static std::size_t totalRead  = 0;
static std::size_t totalWrote = 0;

static auto getStartTime() {
  static auto start_time
    = std::chrono::system_clock::now();
  return start_time;
}

static void printStats() {
  ++totalCount;
  fmt::print("\r"
  "Read  {} bytes, "
  "Wrote {} bytes.",
    totalRead, totalWrote);
  
  if (totalCount % 8 == 0) {
    auto start = getStartTime();
    auto now = std::chrono::system_clock::now();
    fmt::println("  [{:%H:%M:%S}]", now - start);
    totalCount = 0;
  }
}

static int closeFile(std::FILE* fp) {
  fmt::print("\n");
  return std::fclose(fp);
}

#else

ALWAYS_INLINE static int closeFile(std::FILE* fp) {
  return std::fclose(fp);
}

#endif

static std::size_t readFromFP(void* buf, std::size_t count, void* stream) {
  auto* const fp = static_cast<std::FILE*>(stream);
  const auto ret = std::fread(buf, sizeof(Char), count, fp);
#if PRINT_STATS
  totalRead += count;
  printStats();
#endif
  return ret;
}

static std::size_t writeToFP(void* buf, std::size_t count, void* stream) {
  auto* const fp = static_cast<std::FILE*>(stream);
  const auto ret = std::fwrite(buf, sizeof(Char), count, fp);
#if PRINT_STATS
  totalWrote += count;
  printStats();
#endif
  return ret;
}

static Str getFilename(const fs::path& name) {
  return to_multibyte(fs::absolute(name).native());
}

Error IBinaryBuffer::readFile(const fs::path& name) {
  this->destroyStream();
  const auto filename = getFilename(name);
  auto* fp = std::fopen(filename.c_str(), "rb");
  if (!fp)
    return Error::From("Unable to open file to read.");
  
  this->stream_type = StreamType::RFile;
  this->ioStrm.readWriteToStream = &readFromFP;
  this->ioStrm.stream = fp;

  return Error::Ok();
}

Error IBinaryBuffer::writeFile(const fs::path& name) {
  this->destroyStream();
  const auto filename = getFilename(name);
  auto* fp = std::fopen(filename.c_str(), "wb");
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
  switch (stream_type) {
   case StreamType::RFile:
   case StreamType::WFile: {
    auto* const fp = static_cast<std::FILE*>(stream.stream);
    std::fclose(fp);
    break;
   }
   default:
    break;
  }
  stream = exip::IOStream{};
  this->stream_type = StreamType::None;
}
