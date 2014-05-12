#ifdef STANDALONE_TEST

#ifdef message
#undef message
#define message(x)
#else
#define message(x)
#endif
#ifdef ensure
#undef ensure
#define ensure(x,y)
#else
#define ensure(x,y)
#endif
#ifndef NEW
#define NEW new
#endif

#endif
#include "memorymap.hpp"

using namespace nanos;

const char* MemoryChunk::strOverlap[] = {
   "NO_OVERLAP",
   "BEGIN_OVERLAP",
   "END_OVERLAP",
   "TOTAL_OVERLAP",
   "SUBCHUNK_OVERLAP",
   "TOTAL_BEGIN_OVERLAP",
   "SUBCHUNK_BEGIN_OVERLAP",
   "TOTAL_END_OVERLAP",
   "SUBCHUNK_END_OVERLAP"
};

#if 1
void MemoryMap< uint64_t >::insertWithOverlapButNotGenerateIntersects( const MemoryChunk &key, iterator &hint, uint64_t data )
{
   bool lastChunk = false;
   MemoryChunk iterKey = key;
   bool reuseFirstPos = false;
   int firstCase = 0;
   
   if ( !this->empty() )
   {
      if ( hint != this->begin() )
         hint--;
      iterator firstPos = hint;

      MemoryChunk::OverlapType hintOverlap = hint->first.checkOverlap( iterKey );
      if ( hintOverlap == MemoryChunk::NO_OVERLAP && iterKey.getAddress() > hint->first.getAddress()) {
         hint++; firstPos++;
      }

      switch ( hintOverlap )
      {
         case MemoryChunk::NO_OVERLAP:
               firstCase = __LINE__;
               lastChunk = true;
            break;
         case MemoryChunk::BEGIN_OVERLAP:
            firstCase = __LINE__;
            lastChunk = true; //this is a final situation since the to-be-inserted chunk ends.
            break;
         case MemoryChunk::END_OVERLAP:
            firstCase = __LINE__;
            reuseFirstPos = true;
            break;
         case MemoryChunk::TOTAL_OVERLAP:
            break;
         case MemoryChunk::SUBCHUNK_OVERLAP:
            firstCase = __LINE__;
            reuseFirstPos = true;
            lastChunk = true;
            break;
         case MemoryChunk::TOTAL_BEGIN_OVERLAP:
            firstCase = __LINE__;
            reuseFirstPos = true;
            break;
         case MemoryChunk::SUBCHUNK_BEGIN_OVERLAP:
            firstCase = __LINE__;
            reuseFirstPos = true;
            lastChunk = true;
            break;
         case MemoryChunk::TOTAL_END_OVERLAP:
            firstCase = __LINE__;
            lastChunk = true;
            break;
         case MemoryChunk::SUBCHUNK_END_OVERLAP:
            firstCase = __LINE__;
            reuseFirstPos = true;
            lastChunk = true;
            break;
      }

      bool expandToLast = false;
      bool dontDeleteLast = false;

      while ( hint != this->end() && !lastChunk  )  {
         hint++;
         switch ( hint->first.checkOverlap( iterKey ) )
         {
            case MemoryChunk::NO_OVERLAP:
               if ( iterKey.getAddress() < hint->first.getAddress() ) {
                  lastChunk = true;
                  dontDeleteLast = true;
               } else {
                  std::cerr <<"ERROR @ "<< __FUNCTION__<<":"<<__LINE__ << std::endl;
               }
               break;
            case MemoryChunk::BEGIN_OVERLAP:
               expandToLast = true;
               lastChunk = true; //this is a final situation since the to-be-inserted chunk ends.
               break;
            case MemoryChunk::END_OVERLAP: //impossible here
               std::cerr <<"ERROR @ "<< __FUNCTION__<<":"<<__LINE__ << std::endl;
               break;
            case MemoryChunk::TOTAL_OVERLAP:
               break;
            case MemoryChunk::SUBCHUNK_OVERLAP:
               std::cerr <<"ERROR @ "<< __FUNCTION__<<":"<<__LINE__ <<" first case "<< firstCase<< std::endl;
               lastChunk = true;
               break;
            case MemoryChunk::TOTAL_BEGIN_OVERLAP:
               break;
            case MemoryChunk::SUBCHUNK_BEGIN_OVERLAP:
               expandToLast = true;
               lastChunk = true;
               break;
            case MemoryChunk::TOTAL_END_OVERLAP:
               lastChunk = true;
               break;
            case MemoryChunk::SUBCHUNK_END_OVERLAP:
               std::cerr <<"ERROR @ "<< __FUNCTION__<<":"<<__LINE__ << std::endl;
               lastChunk = true;
               break;
         }
      }

      if ( firstPos == hint ) { //range is [firstPos, firstPos)
         if ( reuseFirstPos ) { //overlap with the firstPos, and we must reuse it so don't insert.
            MemoryChunk &firstChunk = const_cast< MemoryChunk & >( hint->first );
            firstChunk.expandIncluding( iterKey );
         } else { //no overlap, and we can insert
            hint = this->insert( hint, BaseMap::value_type( iterKey, data ) );
         }
      } else { //range is [firstPos, hint)
         if ( reuseFirstPos ) { //don't delete first pos
            MemoryChunk &firstChunk = const_cast< MemoryChunk & >( firstPos->first );
            iterator secondPos = firstPos;
            secondPos++;
            firstChunk.expandIncluding( ( expandToLast ? hint->first : iterKey ) );
            if ( !dontDeleteLast ) hint++; //include hint into the deleted range
            this->erase( secondPos, hint );
         } else {
            if ( expandToLast ) iterKey.expandIncluding( hint->first );
            this->insert( firstPos, BaseMap::value_type( iterKey, data ) );
            if ( !dontDeleteLast ) hint++; //include hint into the deleted range
            this->erase( firstPos, hint );
         }
      }
   } else {
      hint = this->insert( hint, BaseMap::value_type( iterKey, data ) );
   }
}

void MemoryMap< uint64_t >::addChunk( uint64_t addr, std::size_t len, uint64_t data )
{
   MemoryChunk key( addr, len );

   iterator it = this->lower_bound( key );
   if ( it == this->end() || this->key_comp()( key, it->first ) || it->first.getLength() != len )
   {
      /* NOT EXACT ADDR FOUND: "addr" is higher than any odther addr in the map OR less than "it" */
      insertWithOverlapButNotGenerateIntersects( key, it, data );
   }
}
#endif