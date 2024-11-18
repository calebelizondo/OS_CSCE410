/*
     File        : nonblocking_disk.c

     Author      : 
     Modified    : 

     Description : 

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

    /* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "nonblocking_disk.H"
#include "simple_disk.H"
#include "machine.H"
#include "thread.H"
#include "scheduler.H"

extern Scheduler *SYSTEM_SCHEDULER;

/*--------------------------------------------------------------------------*/
/* CONSTRUCTOR */
/*--------------------------------------------------------------------------*/

NonBlockingDisk::NonBlockingDisk(DISK_ID _disk_id, unsigned int _size) 
  : SimpleDisk(_disk_id, _size) {
}

/*--------------------------------------------------------------------------*/
/* SIMPLE_DISK FUNCTIONS */
/*--------------------------------------------------------------------------*/

void NonBlockingDisk::fufill_read(void* _args) {
  NonBlockingDisk::DiskFufillArgs* args = (DiskFufillArgs*) _args;
  NonBlockingDisk* disk = args->disk;
  unsigned char* _buf = args->buf;

  while(true) {
    if (disk->is_ready()) {
      int i;
      unsigned short tmpw;
      for (i = 0; i < 256; i++) {
        tmpw = Machine::inportw(0x1F0);
        _buf[i * 2] = (unsigned char)tmpw;
        _buf[i * 2 + 1] = (unsigned char)(tmpw >> 8);
      }

      return;
    } else {
      SYSTEM_SCHEDULER->yield();
    }
  }
}

void NonBlockingDisk::fufill_write(void* _args) {
  NonBlockingDisk::DiskFufillArgs* args = (DiskFufillArgs*) _args;
  NonBlockingDisk* disk = args->disk;
  unsigned char* _buf = args->buf;

  while (true) {
    if (disk->is_ready()) {
      int i;
      unsigned short tmpw;
      for (i = 0; i < 256; i++) {
        tmpw = _buf[2 * i] | (_buf[2 * i + 1] << 8);
        Machine::outportw(0x1F0, tmpw);
      }
      return;
    } else {
      SYSTEM_SCHEDULER->yield();
    }
  }
}


void NonBlockingDisk::read(unsigned long _block_no, unsigned char * _buf) {
  issue_operation(DISK_OPERATION::READ, _block_no);
  char* stack = new char[1024];
  NonBlockingDisk::DiskFufillArgs* args = new NonBlockingDisk::DiskFufillArgs();
  args->disk = this;
  args->buf = _buf; 
  Thread* thread = new Thread(fufill_read, stack, 1024, args);
  SYSTEM_SCHEDULER->add(thread);
}



void NonBlockingDisk::write(unsigned long _block_no, unsigned char * _buf) {
  issue_operation(DISK_OPERATION::WRITE, _block_no);
  char* stack = new char[1024];
  NonBlockingDisk::DiskFufillArgs* args = new NonBlockingDisk::DiskFufillArgs();
  args->disk = this;
  args->buf = _buf; 
  Thread* thread = new Thread(fufill_write, stack, 1024, args);
  SYSTEM_SCHEDULER->add(thread);
}
