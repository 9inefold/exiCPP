#include <cstddef>
#include <cstdint>

extern "C" {
#include <EXIParser.h>
}

namespace exi {
  using CBinaryBuffer = ::BinaryBuffer;

  struct IBinaryBuffer {
    struct AlignAndSize {
      std::size_t size;
      char data[1];
    };

  public:
    CBinaryBuffer getCBuffer();
  };

  template <std::size_t N>
  struct BinaryBuffer : IBinaryBuffer {
    std::size_t size = N;
    char data[N];
  };

  inline CBinaryBuffer IBinaryBuffer::getCBuffer() {
    using CastType = IBinaryBuffer::AlignAndSize*;
    auto* proxy_ptr = reinterpret_cast<CastType>(this);
    return { proxy_ptr->data, proxy_ptr->size, 0 };
  }

  static_assert(
    sizeof(IBinaryBuffer::AlignAndSize) == sizeof(BinaryBuffer<1>)
  );
}

int main() {
  Parser parser;
  exi::BinaryBuffer<512> buffer;
  initParser(&parser, buffer.getCBuffer(), nullptr);
}
