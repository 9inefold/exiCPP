//===- Support/SafeAlloc.cpp ----------------------------------------===//
//
// Copyright (C) 2024-2026 Ninefold
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

#include <Support/SafeAlloc.hpp>
#if EXI_USE_MIMALLOC
# include <mimalloc.h>
#else
# include <new>
#endif

using namespace exi;

EXI_RETURNS_NONNULL EXI_RETURNS_NOALIAS
void* exi::allocate_buffer(usize size, usize align) {
#if EXI_USE_MIMALLOC
  return ::mi_malloc_aligned(size, align);
#else
  return ::operator new(size
# ifdef __cpp_aligned_new
    , std::align_val_t(align)
# endif
  );
#endif
}

void exi::deallocate_buffer(void* ptr, usize size, usize align) {
#if EXI_USE_MIMALLOC
  return ::mi_free(ptr);
#else
  ::operator delete(ptr
# ifdef __cpp_sized_deallocation
    , size
# endif
# ifdef __cpp_aligned_new
    , std::align_val_t(align)
# endif
  );
#endif
}
