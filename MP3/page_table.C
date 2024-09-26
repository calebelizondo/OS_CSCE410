#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   //assert(false);
   PageTable::kernel_mem_pool = _kernel_mem_pool;
   PageTable::process_mem_pool = _process_mem_pool;
   PageTable::shared_size = _shared_size;

   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
   //request first frame to store directory
   this->page_directory = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);
   
   //request frame to store table
   unsigned long * page_table = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);
   
   unsigned long address = 0; 
   unsigned long shared_frames_n = PageTable::shared_size / PAGE_SIZE;

   for (int i = 0; i < shared_frames_n; i++){
      page_table[i] = address | PAGE_WRITE | PAGE_PRESENT;
      address += PAGE_SIZE;
   }

   page_directory[0] = (unsigned long) page_table | PAGE_WRITE | PAGE_PRESENT;

   address = 0;

   for (int i = 1; i < shared_frames_n; i++) {
      page_directory[i] = address | PAGE_WRITE;
   }

   Console::puts("Constructed Page Table object\n");
}


void PageTable::load()
{
   //assert(false);
   PageTable::current_page_table = this;
   write_cr3((unsigned long) page_directory);

   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   //assert(false);
   PageTable::paging_enabled = 1;
   write_cr0(read_cr0() | 0x80000000);
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
  assert(false);
  Console::puts("handled page fault\n");
}

