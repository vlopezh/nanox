#include "system.hpp"
#include "gpudd.hpp"

namespace nanos {

extern "C" {

   void OMPItrace_neventandcounters (unsigned int count, unsigned int *types, unsigned int *values);

   unsigned int nanos_ompitrace_get_max_threads ( void )
   {
#ifdef GPU_DEV
      return sys.getNumPEs() + nanos::ext::GPUDD::getGPUCount();
#else
      return sys.getNumPEs();
#endif
   }

   unsigned int nanos_ompitrace_get_thread_num ( void )
   { 
      if ( myThread == NULL ) return 0;
      else return myThread->getId(); 
   }

}

} // namespace nanos
