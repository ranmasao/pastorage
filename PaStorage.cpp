#include "PaStorage.hpp"

#ifdef __linux

#include <sys/mman.h>

void* aligned_malloc( const size_t size ) {
  void* ptr = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
  if ( ptr == MAP_FAILED ) {
    throw std::runtime_error( "mmap fail" );
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

#elif defined(_WIN32)

#include <windows.h>

void* aligned_malloc( const size_t size ) {
  void* ptr = VirtualAlloc( nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
  if ( ptr == nullptr ) {
    throw std::runtime_error( "VirtualAlloc fail" );
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

void aligned_free( void* ptr, [[maybe_unused]]const size_t size ) {
  if ( VirtualFree( ptr, 0, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS ) == 0 ) {
    throw std::runtime_error( "VirtualFree fail" );
  }
}

#else

#error "Check your platform and/or check and/or fix macros for platform-conditional compilation in this module"

#endif