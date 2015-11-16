#include <stdint.h>
#include "globalregt_decl.hpp"
#include "newregiondirectory.hpp"
#include "regiondict.hpp"
#include "basethread.hpp"
#include "debug.hpp"
#include "router.hpp"
#include "printbt_decl.hpp"

uint64_t global_reg_t::getKeyFirstAddress() const {
   return getFirstAddress( key->getKeyBaseAddress() );
}

uint64_t global_reg_t::getRealFirstAddress() const {
   uint64_t addr = 0;
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   //ensure(entry != NULL, "invalid entry.");
   if ( entry == NULL ) {
      //std::cerr << "Warning, getRealFirstAddress() w/NULL entry!" << std::endl;
      addr = getFirstAddress( key->getRealBaseAddress() );
   } else {
      addr = entry->getBaseAddress() == 0 ? key->getRealBaseAddress() : entry->getBaseAddress();
      if ( addr != 0 ) {
         addr = getFirstAddress( addr );
      }
   }
   return addr;
}

uint64_t global_reg_t::getFirstAddress( uint64_t baseAddress ) const {
   RegionNode *n = key->getRegionNode( id );
   uint64_t offset = 0;
   std::vector< std::size_t > const &sizes = key->getDimensionSizes();
   uint64_t acumSizes = 1;

   for ( unsigned int dimIdx = 0; dimIdx < key->getNumDimensions() - 1; dimIdx += 1 ) {
      acumSizes *= sizes[ dimIdx ];
   }
   
   for ( int dimIdx = key->getNumDimensions() - 1; dimIdx >= 0; dimIdx -= 1 ) {
      //std::size_t accessedLength = n->getValue();
      n = n->getParent();
      std::size_t lowerBound = n->getValue();
      n = n->getParent();
      offset += acumSizes * lowerBound;
      if ( dimIdx >= 1 ) acumSizes = acumSizes / sizes[ dimIdx - 1 ];
   }
   return baseAddress + offset; 
}

std::size_t global_reg_t::getBreadth() const {
   RegionNode *n = key->getRegionNode( id );
   std::size_t offset = 0;
   std::size_t lastOffset = 0;
   std::vector< std::size_t > const &sizes = key->getDimensionSizes();
   uint64_t acumSizes = 1;

   for ( unsigned int dimIdx = 0; dimIdx < key->getNumDimensions() - 1; dimIdx += 1 ) {
      acumSizes *= sizes[ dimIdx ];
   }
   
   for ( int dimIdx = key->getNumDimensions() - 1; dimIdx >= 0; dimIdx -= 1 ) {
      std::size_t accessedLength = n->getValue();
      n = n->getParent();
      std::size_t lowerBound = n->getValue();
      n = n->getParent();
      offset += acumSizes * lowerBound;
      lastOffset += acumSizes * ( lowerBound + accessedLength - 1 );
      if ( dimIdx >= 1 ) acumSizes = acumSizes / sizes[ dimIdx - 1 ];
   }
   return ( lastOffset - offset ) + 1;
}

std::size_t global_reg_t::getDataSize() const {
   RegionNode *n = key->getRegionNode( id );
   std::size_t dataSize = 1;

   for ( int dimIdx = key->getNumDimensions() - 1; dimIdx >= 0; dimIdx -= 1 ) {
      std::size_t accessedLength = n->getValue();
      n = n->getParent();
      n = n->getParent();
      dataSize *= accessedLength;
   }
   return dataSize; 
}

unsigned int global_reg_t::getNumDimensions() const {
   return key->getNumDimensions();
}

global_reg_t::global_reg_t( reg_t r, reg_key_t k ) : id( r ), key( k ) {
}

global_reg_t::global_reg_t( reg_t r, const_reg_key_t k ) : id( r ), ckey( k ) {
}

global_reg_t::global_reg_t() : id( 0 ), key( NULL ) {
}

void global_reg_t::fillDimensionData( nanos_region_dimension_internal_t region[]) const {
   RegionNode *n = key->getRegionNode( id );
   std::vector< std::size_t > const &sizes = key->getDimensionSizes();
   for ( int dimIdx = key->getNumDimensions() - 1; dimIdx >= 0; dimIdx -= 1 ) {
      std::size_t accessedLength = n->getValue();
      n = n->getParent();
      std::size_t lowerBound = n->getValue();
      n = n->getParent();
      region[ dimIdx ].accessed_length = accessedLength;
      region[ dimIdx ].lower_bound = lowerBound;
      region[ dimIdx ].size = sizes[ dimIdx ];
   }
}


bool global_reg_t::operator<( global_reg_t const &reg ) const {
   bool result;
   if ( key < reg.key )
      result = true;
   else if ( reg.key < key )
      result = false;
   else result = ( id < reg.id );
   return result;
}

bool global_reg_t::operator!=( global_reg_t const &reg ) const {
   bool result = false;
   if ( key != reg.key )
      result = true;
   else if ( reg.key != key )
      result = true;
   return result;
}

memory_space_id_t global_reg_t::getFirstLocation() const {
   return NewNewRegionDirectory::getFirstLocation( key, id );
}

memory_space_id_t global_reg_t::getPreferedSourceLocation( memory_space_id_t dest ) const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   memory_space_id_t selected;
   if ( entry->isLocatedIn( dest ) ) {
      selected = dest;
      printBt(std::cerr);
      fatal("Data already in destination.");
   } else {
      selected = sys.getRouter().getSource( dest, entry->getLocations() );
   }
   return selected;
}

unsigned int global_reg_t::getHostVersion( bool increaseVersion ) const {
   unsigned int version = 0;
   if ( NewNewRegionDirectory::isLocatedIn( key, id, (memory_space_id_t) 0 ) ) {
      version = NewNewRegionDirectory::getVersion( key, id, increaseVersion );
   }
   return version;
}

uint64_t global_reg_t::getRealBaseAddress() const {
   return key->getRealBaseAddress();
}

reg_t global_reg_t::getFitRegionId() const {
   RegionNode *n = key->getRegionNode( id );
   bool keep_fitting = true;
   nanos_region_dimension_internal_t fitDimensions[ key->getNumDimensions() ];
   std::vector< std::size_t > const &sizes = key->getDimensionSizes();

   for ( int idx = key->getNumDimensions() - 1; idx >= 0; idx -= 1 ) {
      std::size_t accessedLength = n->getValue();
      n = n->getParent();
      std::size_t lowerBound = n->getValue();
      n = n->getParent();
      fitDimensions[ idx ].size = sizes[ idx ];
      if ( keep_fitting ) {
         fitDimensions[ idx ].accessed_length = accessedLength;
         fitDimensions[ idx ].lower_bound = lowerBound;
         if ( accessedLength != 1 )
            keep_fitting = false;
      } else {
         fitDimensions[ idx ].lower_bound = 0;
         fitDimensions[ idx ].accessed_length = sizes[ idx ];
      }
   }
   return key->obtainRegionId( fitDimensions );
}

reg_t global_reg_t::getSlabRegionId( std::size_t slabSize ) const {
   RegionNode *n = key->getRegionNode( id );
   std::vector< std::size_t > const &sizes = key->getDimensionSizes();
   std::size_t acc_size = 1;
   nanos_region_dimension_internal_t fitDimensions[ key->getNumDimensions() ];
   if ( slabSize < this->getBreadth() ) {
      fatal("Can not allocate slab for this region. Not supported yet. slabSize "<< slabSize << " breadth " << this->getBreadth());
   } else if ( this->getBreadth() < slabSize && id == 1 ) {
      return id;
   } else {

      unsigned int lower_bounds[key->getNumDimensions()];
      for ( int idx = key->getNumDimensions() - 1; idx >= 0; idx -= 1 ) {
         //std::size_t accessedLength = n->getValue();
         n = n->getParent();
         std::size_t lowerBound = n->getValue();
         n = n->getParent();
         lower_bounds[idx] = lowerBound;
      }

      bool keep_expanding = true;
      for ( unsigned int idx = 0; idx < key->getNumDimensions(); idx += 1 ) {
         fitDimensions[ idx ].size = sizes[ idx ];
         //std::cerr << "This dimension size " << sizes[ idx ] << std::endl;
         if ( keep_expanding ) {
            acc_size *= sizes[ idx ];
         //std::cerr << "This dimension acc_size " << acc_size << " slab size " << slabSize << std::endl;
            if ( slabSize == acc_size || ( slabSize > acc_size && ( ( slabSize % acc_size ) == 0 ) ) ) {
               fitDimensions[ idx ].lower_bound = 0;
               fitDimensions[ idx ].accessed_length = sizes[ idx ];
            } else if ( slabSize < acc_size && ( acc_size % slabSize ) == 0 ) {
               std::size_t slab_elems = slabSize / ( acc_size / sizes[ idx ] );
               //std::cerr << "slab_elems is " << slab_elems << " lb: " <<  lower_bounds[idx] << std::endl;
               fitDimensions[ idx ].accessed_length = slab_elems;
               fitDimensions[ idx ].lower_bound = ( lower_bounds[idx] / slab_elems ) * slab_elems;
               keep_expanding = false;
            } else {
               fatal("invalid slabSize: " << slabSize << " reg size is " << this->getBreadth() );
            }
         } else {
            fitDimensions[ idx ].accessed_length = 1;
            fitDimensions[ idx ].lower_bound = 1;
         }
      }
      (void ) fitDimensions;
   }
   return key->obtainRegionId( fitDimensions );
}

void global_reg_t::initializeGlobalEntryIfNeeded() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   if ( entry == NULL ) {
      NewNewRegionDirectory::initializeEntry( key, id );
   }
}

void global_reg_t::setLocationAndVersion( ProcessingElement *pe, memory_space_id_t loc, unsigned int version ) const {
   NewNewRegionDirectory::addAccess( key, id, pe, loc, version );
}

DeviceOps *global_reg_t::getDeviceOps() const {
   return NewNewRegionDirectory::getOps( key, id );
}

DeviceOps *global_reg_t::getHomeDeviceOps() {
   DeviceOps *ops = NULL;
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   memory_space_id_t home = (entry->getRootedLocation() == (unsigned int) -1) ? 0 : entry->getRootedLocation();
   if ( home == 0 ) {
      ops = NewNewRegionDirectory::getOps( key, id );
   } else {
      AllocatedChunk *chunk = sys.getSeparateMemory( home ).getCache().getAllocatedChunk( *this, *((WD *)NULL), (unsigned int)-1 );
      ops = chunk->getDeviceOps(*this, *((WD *)NULL), (unsigned int)-1 );
      chunk->unlock();
   }
   return ops;
}

bool global_reg_t::contains( global_reg_t const &reg ) const {
   bool result = false;
   if ( key == reg.key ) {
      if ( id == reg.id || ( key->checkIntersect( id, reg.id ) && ( reg.id == key->computeIntersect( id, reg.id ) ) ) ) {
         result = true;
      }
   }
   return result;
}

bool global_reg_t::isLocatedIn( memory_space_id_t loc ) const {
   return NewNewRegionDirectory::isLocatedIn( key, id, loc );
}

unsigned int global_reg_t::getVersion() const {
   return NewNewRegionDirectory::getVersion( key, id, false );
}

void global_reg_t::fillCopyData( CopyData &cd, uint64_t baseAddress ) const {
   cd.setBaseAddress( (void *) baseAddress );
   cd.setRemoteHost( true );
   cd.setHostBaseAddress( key->getKeyBaseAddress() );
   cd.setNumDimensions( key->getNumDimensions() );
}

bool global_reg_t::isRegistered() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   return entry != NULL;
}

std::set< memory_space_id_t > const &global_reg_t::getLocations() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   return entry->getLocations();
}

//void global_reg_t::setRooted() const {
//   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
//   ensure(entry != NULL, "invalid entry.");
//   entry->setRooted();
//}

memory_space_id_t global_reg_t::getRootedLocation() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   return entry->getRootedLocation();
}

bool global_reg_t::isRooted() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   return entry->isRooted();
}
void global_reg_t::setOwnedMemory(memory_space_id_t loc) const {
   //setRooted();
   NewNewRegionDirectory::addRootedAccess( key, id, loc, 2 );
}

unsigned int global_reg_t::getNumLocations() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   return entry->getLocations().size();
}

ProcessingElement *global_reg_t::getFirstWriterPE() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   return entry->getFirstWriterPE();
}

bool global_reg_t::isLocatedInSeparateMemorySpaces() const {
   NewNewDirectoryEntryData *entry = NewNewRegionDirectory::getDirectoryEntry( *key, id );
   ensure(entry != NULL, "invalid entry.");
   std::set< memory_space_id_t > const &locs = entry->getLocations();
   return ( locs.size() > 1 || locs.count(0) == 0 ); 
}
