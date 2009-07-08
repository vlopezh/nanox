#include "system.hpp"
#include "coresetup.hpp"
#include "smpprocessor.hpp"
#include "schedule.hpp"

using namespace nanos;

System nanos::sys;

System::System ()
{

  CoreSetup::prepareConfig(config);
  SMPProcessor::prepareConfig(config);
  
  init();
  start();
}

void System::init ()
{
    config.init();
}

//SchedulingGroup * createBreadthFirstPolicy();
SchedulingGroup * createTaskStealPolicy(int);

void System::start ()
{
    int numPes = CoreSetup::getNumPEs();

    // if preload, TODO: allow dynamic PE creation

    // Reserve seems to make it crash at exit :?
    //pes.reserve(numPes);

    //TODO: remove, initialize policy dynamically
    SchedulingGroup *sg = createTaskStealPolicy(numPes);
    //SchedulingGroup *sg = createBreadthFirstPolicy();
    //TODO: decide, single master, multiple master start

    PE *pe = new SMPProcessor(0);
    pes.push_back(pe);
    pe->associateThisThread(sg);

    for ( int p = 1; p < numPes ; p++ ) {
	// TODO: create processor type based on config
      pe = new SMPProcessor(p);
      pes.push_back(pe);
      pe->startWorker(sg);
    }
}

System::~System ()
{
   verbose("NANOS++ shutting down.... init");

   verbose("Wait for main workgroup to complete");
   myThread->getCurrentWD()->waitCompletation();

   verbose("Joining threads... phase 1");
   // signal stop PEs
   for ( unsigned p = 1; p < pes.size() ; p++ ) {
      pes[p]->stopAll();
   }
   verbose("Joining threads... phase 2");
   // join
   for ( unsigned p = 1; p < pes.size() ; p++ ) {
      delete pes[p];
   }
   verbose("NANOS++ shutting down.... end");
}

//TODO: remove?
void System::submit (WD &work, WorkDescriptor * parent)
{
	 //ADDED parent setting for cilk scheduler
	work.setParent(parent);

	Scheduler::submit(work);
}

//TODO: void system_description ()
// {
// 	// Altix simple
// 	System = new UMASystem ();
// 	System.addN(128, SmpPE(1594));
//
// 	// Altix complex
// 	System = new NUMASystem();
// 	System.add(64, Node(MultiCore(2,SmpPE(1594)),1GB) );
//
// 	// Cell
// 	System = new NUMASystem();
// 	System.add(1,Node(MultiCore(2,SmpPE(1000)));
// 	System.add(8,Node(SpuPE(),256K));
//
// 	// MN Partition
// 	System = new ClusterSystem();
// 	System.add(10,UMASystem(2,Multicore(2,SmpPE(1000))));
//
// 	// MI Partition
// 	System = new ClusterSystem();
// 	System.add(10,NUMASystem(1,MultiCore(2,SmpPE(2000)),16,SpuPE());
// }
