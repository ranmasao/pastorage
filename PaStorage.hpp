#ifndef __PA_STORAGE_HPP__
#define __PA_STORAGE_HPP__

#include <cstdint>
#include <cstring>
#include <stdexcept>

void* aligned_malloc( const size_t size );
void* aligned_realloc( void* ptr, const size_t oldSize, const size_t newSize );
void  aligned_free( void* prt, const size_t size );

const size_t ELEMENTS_ALIGNMENT = sizeof( void* ); // should be 4 for 32-bit platform or 8 for 64-bit
static_assert( ( ELEMENTS_ALIGNMENT == 4 || ELEMENTS_ALIGNMENT == 8 ), "Pointer size supposed to be 4 or 8 bytes" );
const size_t DEFAULT_RAM_PAGE_SIZE = 4096;
const size_t ELEMENTS_PER_BLOCK = 1024;
const size_t MAX_TOTAL_ELEMENTS = SIZE_MAX;
const size_t POINTERS_PER_RAM_PAGE = DEFAULT_RAM_PAGE_SIZE / ELEMENTS_ALIGNMENT;
#if (__SIZEOF_POINTER__ == __SIZEOF_LONG_LONG__)
    #define tFlagBaseType unsigned long long
    #define FFS ffsll
    #define FLAG_EMPTY 0xffffffffffffffffull
    #define FLAG_FULL 0ull
#elif (__SIZEOF_POINTER__ == __SIZEOF_LONG__)
    #define tFlagBaseType unsigned long
    #define FFS ffsl
    #define FLAG_EMPTY 0xfffffffful
    #define FLAG_FULL 0ul
#else
    #error "Unsupported pointer size"
#endif
const size_t FLAG_BASE_TYPE_SIZE = sizeof( tFlagBaseType );
const size_t FLAGS_PER_BASE_ELEMENT = FLAG_BASE_TYPE_SIZE * 8;
const size_t FLAGS_BASE_ELEMENTS_PER_PAGE = DEFAULT_RAM_PAGE_SIZE / FLAG_BASE_TYPE_SIZE;
const size_t FLAGS_PER_PAGE = DEFAULT_RAM_PAGE_SIZE / 8;

template < typename T >
class cStorage {
public:
  class iterator {
  public:
    iterator( cStorage< T >& cstorage, size_t id ): storageObject( cstorage ), id( id ) {};
    iterator operator++() {
      ++id;
      return *this;
    };
    bool operator!=( const iterator& other ) const {
      return &storageObject != &other.storageObject || id != other.id;
    };
    const T& operator*() const {
      return storageObject[ id ];
    };
    T& operator*() {
      return storageObject[ id ];
    };
  private:
    cStorage< T >& storageObject;
    size_t id;
  };

  cStorage() {
    elementSize = sizeof( T );
    if ( elementSize % ELEMENTS_ALIGNMENT == 0 ) {
      elementSizeAligned = elementSize;
    } else {
      elementSizeAligned = elementSize + ELEMENTS_ALIGNMENT - ( elementSize % ELEMENTS_ALIGNMENT );
    }
    blockOfElementsSize = elementSizeAligned * ELEMENTS_PER_BLOCK;
  };

  ~cStorage() {
    if ( blocks != nullptr ) {
      for ( size_t i = 0; i < usedBlocksPtrs; i++ ) {
        aligned_free( blocks[ i ], blockOfElementsSize );
      }
    }
    aligned_free( blocks, allocatedBlocksPagesSize );
  }

  size_t push_back( const T& x ) {
    size_t elementIndex;
    if ( availableElements == 0 ) {
      AddBlock();
    }
    elementIndex = firstUnusedElement;
    size_t blockIndex = elementIndex / ELEMENTS_PER_BLOCK;
    size_t elementIndexInBlock = elementIndex % ELEMENTS_PER_BLOCK;
    T* elementAddress = reinterpret_cast< T* >(
      reinterpret_cast< uintptr_t >( blocks[ blockIndex ] ) + elementSizeAligned * elementIndexInBlock
    );
    memcpy( elementAddress, &x, elementSize );
    SetElementUsed( elementIndex );
    return elementIndex;
  };
  size_t capacity() const {
    return usedBlocksPtrs * ELEMENTS_PER_BLOCK;
  };
  size_t size() const {
    return usedElements;
  }

  iterator back() { // very questionable
    return iterator( *this, usedElements - 1 );
  };
  iterator begin() {
    return iterator( *this, 0 );
  };
  iterator end() { // also questionable
    return iterator( *this, usedElements );
  };

  T& operator[]( size_t index ) {
    if ( !IsElementUsed( index ) ) {
      SetElementUsed( index );
    }
    size_t blockIndex = index / ELEMENTS_PER_BLOCK;
    size_t elementIndexInBlock = index % ELEMENTS_PER_BLOCK;
    return *( reinterpret_cast< T* >(
      reinterpret_cast< uintptr_t >( blocks[ blockIndex ] ) + elementSizeAligned * elementIndexInBlock
    ) );
  }

  bool IsElementUsed( const size_t index ) {
    if ( index > usedBlocksPtrs * ELEMENTS_PER_BLOCK ) {
      throw std::out_of_range( "operator[] was called for unallocated space" );
    }
    size_t flagIndex = index / FLAGS_PER_BASE_ELEMENT;
    size_t flagBitIndex = index % FLAGS_PER_BASE_ELEMENT;
    tFlagBaseType flagMask = ( ( tFlagBaseType )1 ) << flagBitIndex;
    return ( flags[ flagIndex ] & flagMask ) == 0;
  }
private:
  void AddBlockPage() {
    blocks = reinterpret_cast< void** >(
      aligned_realloc( blocks, allocatedBlocksPagesSize, allocatedBlocksPagesSize + DEFAULT_RAM_PAGE_SIZE )
    );
    for ( size_t i = unusedBlocksPtrs; i < unusedBlocksPtrs + POINTERS_PER_RAM_PAGE; i++ ) {
      blocks[ i ] = nullptr;
    }
    allocatedBlocksPagesSize += DEFAULT_RAM_PAGE_SIZE;
    allocatedBlocksPages++;
    unusedBlocksPtrs += POINTERS_PER_RAM_PAGE;
  }

  void AddBlock() {
    if ( unusedBlocksPtrs == 0 ) {
      AddBlockPage();
    }
    blocks[ usedBlocksPtrs ] = aligned_malloc( blockOfElementsSize );
    usedBlocksPtrs++;
    unusedBlocksPtrs--;
    availableElements += ELEMENTS_PER_BLOCK;
    if ( availableElements > allocatedFlags ) {
      AddFlagPage();
    }
  }

  void AddFlagPage() {
    flags = reinterpret_cast< tFlagBaseType* >(
      aligned_realloc( flags, allocatedFlagsSize, allocatedFlagsSize + DEFAULT_RAM_PAGE_SIZE )
    );
    for ( size_t i = allocatedFlagsBaseElements; i < allocatedFlagsBaseElements + FLAGS_BASE_ELEMENTS_PER_PAGE; i++ ) {
      flags[ i ] = FLAG_EMPTY;
    }
    allocatedFlagsSize += DEFAULT_RAM_PAGE_SIZE;
    allocatedFlags += FLAGS_PER_PAGE;
    allocatedFlagsBaseElements += FLAGS_BASE_ELEMENTS_PER_PAGE;
  }

  T* FindNextFreeElement( size_t* resultIndex, size_t _startingIndex = 0 ) {
    if ( availableElements == 0 ) {
      AddBlock();
      void* block = blocks[ usedBlocksPtrs - 1 ];
      return reinterpret_cast< T* >( block );
    } else {
      size_t startingIndex = _startingIndex / FLAGS_PER_BASE_ELEMENT;
      for ( size_t i = startingIndex; i < allocatedFlagsBaseElements; i++ ) {
        int flagNum = -1;
        if ( flags[ i ] == FLAG_FULL ) {
          continue;
        }
        if ( flags[ i ] == FLAG_EMPTY ) {
          flagNum = 0;
        } else {
          if ( i != startingIndex ) { // more likely
            flagNum = FFS( flags[ i ] ) - 1;
          } else { // find next free in current flag element
            size_t startingBit = _startingIndex % FLAGS_PER_BASE_ELEMENT;
            // all bits set to 0 (as busy) from least to startingBit
            tFlagBaseType mask = ~( ( ( ( tFlagBaseType )1 ) << ( startingBit + 1 ) ) - 1 );
            // new flag = old flag with bits set to 0 (as busy) lower or equal to the index
            tFlagBaseType newFlag = flags[ i ] & mask;
            flagNum = FFS( newFlag ) - 1;
          }
        }
        size_t elementIndex = flagNum + i * FLAGS_PER_BASE_ELEMENT;
        if ( resultIndex != nullptr ) {
          *resultIndex = elementIndex;
        }
        size_t blockIndex = elementIndex / ELEMENTS_PER_BLOCK;
        size_t elementIndexInBlock = elementIndex % ELEMENTS_PER_BLOCK;
        return ( reinterpret_cast< T* >(
          reinterpret_cast< uintptr_t >( blocks[ blockIndex ] ) + elementSizeAligned * elementIndexInBlock
        ) );
      }
    }
    return nullptr;
  }

  void SetElementUsed( const size_t index ) {
    size_t flagIndex = index / FLAGS_PER_BASE_ELEMENT;
    size_t flagBitIndex = index % FLAGS_PER_BASE_ELEMENT;
    if ( ( ( flags[ flagIndex ] >> flagBitIndex ) & 1 ) == 1 ) {
      flags[ flagIndex ] ^= ( ( tFlagBaseType )1 << flagBitIndex );
    } else {
      throw std::runtime_error( "attempt to set as busy element which is already busy" );
    }
    if ( availableElements != 0 ) {
      FindNextFreeElement( &firstUnusedElement, firstUnusedElement );
    } else {
      firstUnusedElement = usedBlocksPtrs * ELEMENTS_PER_BLOCK;
    }
    availableElements--;
    usedElements++;
  }

  /*
    Array of pointers to blocks, each block is a contigous memory chunk holding ELEMENTS_PER_BLOCK like POD array.
    This array itself is extending every time by single RAM page.
  */
  void**          blocks = nullptr;
  tFlagBaseType*  flags = nullptr;

  size_t  elementSize = 0;         // single element size
  size_t  elementSizeAligned = 0;  // single element size padded to the alignment
  size_t  blockOfElementsSize = 0; // size of ELEMENTS_PER_BLOCK aligned elements

  size_t  usedElements = 0;       // how many elements are actually used
  size_t  availableElements = 0;  // how many elements are available in allocated blocks (without used ones)
  size_t  allocatedBlocksPages = 0;
  size_t  allocatedBlocksPagesSize = 0;
  size_t  usedBlocksPtrs = 0;
  size_t  unusedBlocksPtrs = 0;   // how many blocks pointers are not used yet
  size_t  allocatedFlags = 0;     // in bits
  size_t  allocatedFlagsSize = 0; // in bytes
  size_t  allocatedFlagsBaseElements = 0; // in choosen type
  size_t  firstUnusedElement = 0;
};

#endif // __PA_STORAGE_HPP__
