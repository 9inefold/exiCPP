#include <cstddef>
#include <cstdint>
#include <exip/EXIParser.h>

namespace exi {
  using CParser = ::Parser;
  using CBinaryBuffer = ::BinaryBuffer;
}

struct AppData {
  unsigned elementCount;
  unsigned nestingLevel;
};

int main() {
  Parser parser;
  // exi::BinaryBuffer<512> buffer;
  AppData data {};

  // initParser(&parser, buffer.getCBuffer(), &data);
  // destroyParser(&parser);
}
