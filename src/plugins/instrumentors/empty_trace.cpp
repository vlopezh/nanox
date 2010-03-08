#include "plugin.hpp"
#include "system.hpp"
#include "instrumentor.hpp"

namespace nanos {

class InstrumentorEmptyTrace: public Instrumentor 
{
#if defined INSTRUMENTATION_ENABLED
   private:
   public:
      // constructor
      InstrumentorEmptyTrace ( ) { }

      // destructor
      ~InstrumentorEmptyTrace ( ) { }

      // low-level instrumentation interface (mandatory functions)

      virtual void initialize( void ) {}
      virtual void finalize( void ) {}
      virtual void addEventList ( unsigned int count, Event *events ) {}

      // high-level events

      virtual void enterRuntimeAPI ( nanos_event_api_t function, nanos_event_state_t state = RUNTIME ) {}
      virtual void leaveRuntimeAPI ( ) {}
      virtual void enterIdle ( ) {}
      virtual void leaveIdle ( ) {}
      virtual void wdCreate ( WorkDescriptor *wd ) {}
      virtual void wdSwitch( WorkDescriptor* oldWD, WorkDescriptor* newWD ) {}
      virtual void wdExit( WorkDescriptor* oldWD, WorkDescriptor* newWD ) {}
#endif
};


namespace ext {

class InstrumentorEmptyTracePlugin : public Plugin {
   public:
      InstrumentorEmptyTracePlugin () : Plugin("Instrumentor which doesn't generate any trace.",1) {}
      ~InstrumentorEmptyTracePlugin () {}

      virtual void config( Config &config ) {}

      void init ()
      {
         sys.setInstrumentor( new InstrumentorEmptyTrace() );	
      }
};

} // namespace ext
} // namespace nanos

nanos::ext::InstrumentorEmptyTracePlugin NanosXPlugin;