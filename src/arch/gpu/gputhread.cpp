/*************************************************************************************/
/*      Copyright 2009 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the NANOS++ library.                                    */
/*                                                                                   */
/*      NANOS++ is free software: you can redistribute it and/or modify              */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      NANOS++ is distributed in the hope that it will be useful,                   */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with NANOS++.  If not, see <http://www.gnu.org/licenses/>.             */
/*************************************************************************************/

#include "gputhread.hpp"
#include "gpuprocessor.hpp"
#include "instrumentationmodule_decl.hpp"
#include "schedule.hpp"
#include "system.hpp"

#include <cuda_runtime.h>
#ifdef NANOS_GPU_USE_CUDA32
extern void cublasShutdown();
extern void cublasSetKernelStream( cudaStream_t );
#else
#include <cublas.h>
#include <cublas_v2.h>
#endif

using namespace nanos;
using namespace nanos::ext;


void * nanos::ext::gpu_bootthread ( void *arg )
{
   GPUThread *self = static_cast<GPUThread *>( arg );

   self->run();

   pthread_exit ( 0 );
}

void GPUThread::start()
{
   pthread_attr_t attr;
   pthread_attr_init( &attr );

   if ( pthread_create( &_pth, &attr, gpu_bootthread, this ) )
      fatal( "couldn't create thread" );
}
void GPUThread::join()
{
   pthread_join( _pth, NULL );
   joined();
}

void GPUThread::switchTo( WD *work, SchedulerHelper *helper )
{
   fatal("A GPUThread cannot call switchTo function.");
}
void GPUThread::exitTo( WD *work, SchedulerHelper *helper )
{
   fatal("A GPUThread cannot call exitTo function.");
}

void GPUThread::switchHelperDependent( WD* oldWD, WD* newWD, void *arg )
{
   fatal("A GPUThread cannot call switchHelperDependent function.");
}



void GPUThread::initializeDependent ()
{
   // Bind the thread to a GPU device
   NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_SET_DEVICE_EVENT );
   cudaError_t err = cudaSetDevice( _gpuDevice );
   NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;
   if ( err != cudaSuccess )
      warning( "Couldn't set the GPU device for the thread: " << cudaGetErrorString( err ) );

   // Initialize GPUProcessor
   ( ( GPUProcessor * ) myThread->runningOn() )->init();

   // Warming up GPU's...
   if ( GPUConfig::isGPUWarmupDefined() ) {
      NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_FREE_EVENT );
      cudaFree(0);
      NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;
   }

#ifndef NANOS_GPU_USE_CUDA32
   // Initialize CUBLAS handle in case of potentially using CUBLAS
   if ( GPUConfig::isCUBLASInitDefined() ) {
      NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_GENERIC_EVENT );
      cublasStatus_t cublasErr = cublasCreate( ( cublasHandle_t * ) &_cublasHandle );
      NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;
      if ( cublasErr != CUBLAS_STATUS_SUCCESS ) {
         if ( cublasErr == CUBLAS_STATUS_NOT_INITIALIZED ) {
            warning( "Couldn't set the context handle for CUBLAS library: the CUDA Runtime initialization failed" );
         } else if ( cublasErr == CUBLAS_STATUS_ALLOC_FAILED ) {
            warning( "Couldn't set the context handle for CUBLAS library: the resources could not be allocated" );
         } else {
            warning( "Couldn't set the context handle for CUBLAS library: unknown error" );
         }
      } else {
         // It seems like it is useless, but still do it in case it works some time...
         // This call is causing a segmentation fault inside CUBLAS library...
         //cublasErr = cublasSetStream( * ( ( cublasHandle_t * ) _cublasHandle ),
         //      ( ( ( GPUProcessor * ) myThread->runningOn() )->getGPUProcessorInfo()->getKernelExecStream() ) );
         //if ( cublasErr != CUBLAS_STATUS_SUCCESS ) {
         //   warning( "Error setting the CUDA stream for the CUBLAS library" );
         //}
      }
   }
#endif

   // Reset CUDA errors that may have occurred inside the runtime initialization
   NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_GET_LAST_ERROR_EVENT );
   err = cudaGetLastError();
   NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;
   if ( err != cudaSuccess )
      warning( "CUDA errors occurred during initialization:" << cudaGetErrorString( err ) );

   // Set the number of look ahead (prefetching) tasks
   // Add one to also count current workdescriptor
   setMaxPrefetch( GPUConfig::getNumPrefetch() + 1 );
}

void GPUThread::runDependent ()
{
   WD &work = getThreadWD();
   setCurrentWD( work );
   SMPDD &dd = ( SMPDD & ) work.activateDevice( SMP );

   if ( getTeam() == NULL ) {
      warning( "This GPUThread needs a team to work, but no team was found. The thread will exit.");
      return;
   }


   dd.getWorkFct()( work.getData() );

   if ( GPUConfig::isCUBLASInitDefined() ) {
#ifdef NANOS_GPU_USE_CUDA32
      cublasShutdown();
#else
      cublasDestroy( ( cublasHandle_t ) _cublasHandle );
#endif
   }

   ( ( GPUProcessor * ) myThread->runningOn() )->cleanUp();
}

//bool GPUThread::inlineWorkDependent ( WD &wd )
bool GPUThread::runWDDependent( WD &wd )
{
   GPUDD &dd = ( GPUDD & )wd.getActiveDevice();
   //GPUProcessor &myGPU = * ( GPUProcessor * ) myThread->runningOn();

#if 0
   if ( GPUConfig::isOverlappingInputsDefined() ) {
      // Wait for the input transfer stream to finish
      NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_INPUT_STREAM_SYNC_EVENT );
      cudaStreamSynchronize( myGPU.getGPUProcessorInfo()->getInTransferStream() );
      NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;
      // Erase the wait input list and synchronize it with cache
      myGPU.getInTransferList()->clearMemoryTransfers();
      myGPU.freeInputPinnedMemory();
   }

   // Check if someone is waiting for our data
   myGPU.getOutTransferList()->clearRequestedMemoryTransfers();

   // We wait for wd inputs, but as we have just waited for them, we could skip this step
   wd.start( WD::IsNotAUserLevelThread );
#endif

   NANOS_INSTRUMENT ( InstrumentStateAndBurst inst1( "user-code", wd.getId(), NANOS_RUNNING ) );
   ( dd.getWorkFct() )( wd.getData() );

#if 0
   if ( !GPUConfig::isOverlappingOutputsDefined() && !GPUConfig::isOverlappingInputsDefined() ) {
      // Wait for the GPU kernel to finish
      NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_DEVICE_SYNC_EVENT );
#ifdef NANOS_GPU_USE_CUDA32
      cudaThreadSynchronize();
#else
      cudaDeviceSynchronize();
#endif
      NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;

      // Normally this instrumentation code is inserted by the compiler in the task outline.
      // But because the kernel call is asynchronous for GPUs we need to raise them manually here
      // when we know the kernel has really finished
      NANOS_INSTRUMENT ( raiseWDClosingEvents() );

      // Copy out results from tasks executed previously
      // Do it always, as another GPU may be waiting for results
      myGPU.getOutTransferList()->executeMemoryTransfers();
   }
   else {
      myGPU.getOutTransferList()->executeMemoryTransfers();
   }

   if ( GPUConfig::isPrefetchingDefined() ) {
      WD * last = &wd;
      while ( canPrefetch() ) {
         // Get next task in order to prefetch data to device memory
         WD *next = Scheduler::prefetch( ( nanos::BaseThread * )  this, *last );
         if ( next != NULL ) {
            next->init();
            addNextWD( next );
            last = next;
         } else {
            break;
         }
      }
   }

   if ( GPUConfig::isOverlappingOutputsDefined() ) {
      NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_OUTPUT_STREAM_SYNC_EVENT );
      cudaStreamSynchronize( myGPU.getGPUProcessorInfo()->getOutTransferStream() );
      NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;
      myGPU.freeOutputPinnedMemory();
   }

   if ( GPUConfig::isOverlappingOutputsDefined() || GPUConfig::isOverlappingInputsDefined() ) {
      // Wait for the GPU kernel to finish, if we have not waited before
      //cudaThreadSynchronize();
      NANOS_GPU_CREATE_IN_CUDA_RUNTIME_EVENT( NANOS_GPU_CUDA_KERNEL_STREAM_SYNC_EVENT );
      cudaStreamSynchronize( myGPU.getGPUProcessorInfo()->getKernelExecStream() );
      NANOS_GPU_CLOSE_IN_CUDA_RUNTIME_EVENT;

      // Normally this instrumentation code is inserted by the compiler in the task outline.
      // But because the kernel call is asynchronous for GPUs we need to raise them manually here
      // when we know the kernel has really finished
      NANOS_INSTRUMENT ( raiseWDClosingEvents() );
   }
#endif

   //return true;
   return false;
}

void GPUThread::yield()
{
   cudaFree(0);
   ( ( GPUProcessor * ) runningOn() )->getInTransferList()->executeMemoryTransfers();
   ( ( GPUProcessor * ) runningOn() )->getOutTransferList()->executeMemoryTransfers();

   AsyncThread::yield();
}

void GPUThread::idle()
{
   cudaFree(0);
   ( ( GPUProcessor * ) runningOn() )->getInTransferList()->executeMemoryTransfers();
   ( ( GPUProcessor * ) runningOn() )->getOutTransferList()->removeMemoryTransfer();

   AsyncThread::idle();
}


void GPUThread::raiseWDClosingEvents ( WD * wd )
{
   //if ( _wdClosingEvents ) {
      NANOS_INSTRUMENT(
            WD * oldwd = getCurrentWD();
            // Instrumenting context switch: oldwd leaves CPU, but will come back later (last = false)
            sys.getInstrumentation()->wdSwitch( oldwd, wd, /* last */ false );
            Instrumentation::Event e[2];
            sys.getInstrumentation()->closeBurstEvent( &e[0],
                  sys.getInstrumentation()->getInstrumentationDictionary()->getEventKey( "user-funct-name" ) );
            sys.getInstrumentation()->closeBurstEvent( &e[1],
                  sys.getInstrumentation()->getInstrumentationDictionary()->getEventKey( "user-funct-location" ) );

            sys.getInstrumentation()->addEventList( 2, e );

            // Instrumenting context switch: wd leaves CPU, but will come back later (last = false)
            sys.getInstrumentation()->wdSwitch( wd, oldwd, /* last */ false );
      );
      //_wdClosingEvents = false;
   //}
}
