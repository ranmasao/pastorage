#include "PaStorage.hpp"

#include <stdexcept>
#include <sys/mman.h>

#ifdef __linux__

void* aligned_malloc( const size_t size ) {
  void* ptr = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
  if ( ptr == MAP_FAILED ) {
    throw std::runtime_error( "mmap fail" );
    return nullptr;
  }
  return ptr;
}

void* aligned_realloc( void* ptr, const size_t oldSize, const size_t newSize ) {
  void* newPtr = aligned_malloc( newSize );
  if ( ptr != nullptr ) {
    memcpy( newPtr, ptr, oldSize );
    aligned_free( ptr, oldSize );
  }
  return newPtr;
}

void aligned_free( void* ptr, const size_t size ) {
  if ( munmap( ptr, size ) == -1 ) {
    throw std::runtime_error( "munmap fail" );
  }
}

#else

#error TODO: Windows implementation

#endif
