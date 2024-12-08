#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"
#include "vm_pool.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   PageTable::kernel_mem_pool = _kernel_mem_pool;
   PageTable::process_mem_pool = _process_mem_pool;
   PageTable::shared_size = _shared_size;

   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
   //request first frame to store directory, translate into address
   this->page_directory = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);
   
   //request frame to store table, translate into address
   unsigned long * page_table = (unsigned long *) (kernel_mem_pool->get_frames(1) * PAGE_SIZE);

   //populate page table
   unsigned long addr = 0; 
   unsigned long shared_frames_n = PageTable::shared_size / PAGE_SIZE;
   for (int i = 0; i < shared_frames_n; i++){
      page_table[i] = addr | PAGE_WRITE | PAGE_PRESENT;
      addr += PAGE_SIZE;
   }

   //populate the page dir
   page_directory[0] = (unsigned long) page_table | PAGE_WRITE | PAGE_PRESENT;
   for (int i = 1; i < shared_frames_n; i++) {
      page_directory[i] = 0; //
   }

   vmpools = nullptr;
   vmpool_n = 0;
   capacity = 0;

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

//handles various protection issues
void PageTable::handle_protection_fault(REGS* r, unsigned long page_dir_offset, unsigned long page_table_offset) {
   unsigned long* page_table = (unsigned long*)(current_page_table->page_directory[page_dir_offset]);
   int pte_flag = page_table[page_table_offset] & 0xC;

   if (!(r->err_code & 0x4)) { // Kernel mode ...
      if ((r->err_code & 0x2) > (pte_flag & 0x2)) {
         Console::puts("kernel touched read-only page (oops)\n");
         assert(false);
      }
   } else { // User mode ...
      if ((r->err_code & 0x4) > (pte_flag & 0x2)) {
         Console::puts("user touched kernel page (oops)\n");
         assert(false);
      } else if ((r->err_code & 0x2) > (pte_flag & 0x2)) {
         Console::puts("user touched read-only page (oops)\n");
         assert(false);
      }
   }
}

void PageTable::handle_not_present_fault(unsigned long page_dir_offset, unsigned long page_table_offset) {
   
   //carry request to appropriate handler ... 
   if (!(current_page_table->page_directory[page_dir_offset] & PAGE_PRESENT)) {
      allocate_page_table(page_dir_offset, page_table_offset);
   } else {
      allocate_page(page_dir_offset, page_table_offset);
   }
}


void PageTable::handle_fault(REGS* r) 
{
   unsigned long fault_address = read_cr2();
   unsigned long page_dir_offset = fault_address >> 22; //first 22 are for the dir offset ...
   unsigned long page_table_offset = (fault_address >> 12) & 0x3FF; //the rest are for the page_table offset

   if (r->err_code & 0x1) { //if there's a protection issue ...
      handle_protection_fault(r, page_dir_offset, page_table_offset);
   } else { // otherwise ... 
      handle_not_present_fault(page_dir_offset, page_table_offset);
   }

   Console::puts("Handled page fault!!!!\n");
}

void PageTable::allocate_page_table(unsigned long page_dir_offset, unsigned long page_table_offset) {

   //request new frame to hold table
   unsigned long new_frame = kernel_mem_pool->get_frames(1);
   if (!new_frame) {
      Console::puts("No frames available for page table!!!!\n");
      assert(false);
   }

   //populate the table
   unsigned long* page_table = (unsigned long*)(new_frame * PAGE_SIZE);
   current_page_table->page_directory[page_dir_offset] = (unsigned long)page_table
       | PAGE_PRESENT | PAGE_WRITE;
   for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
      page_table[i] = 0; 
   }

   allocate_page(page_dir_offset, page_table_offset);
}

void PageTable::allocate_page(unsigned long page_dir_offset, unsigned long page_table_offset) {
   unsigned long new_frame = process_mem_pool->get_frames(1);
   if (!new_frame) {
      Console::puts("no frame in pool available for fault!!!\n");
      assert(false);
   }
   //populate page table
   unsigned long* page_table = (unsigned long*)
      (current_page_table->page_directory[page_dir_offset] & 0xFFFFF000);
   page_table[page_table_offset] = new_frame * PAGE_SIZE | PAGE_PRESENT; 
}

unsigned long* PageTable::PDE_address(unsigned long addr) {
   unsigned long page_dir_index = addr >> 22;
   unsigned long* pde_addr = &(current_page_table->page_directory[page_dir_index]);
   return pde_addr;
}

unsigned long* PageTable::PTE_address(unsigned long addr) {
   unsigned long page_dir_index = addr >> 22;
   unsigned long page_table_index = (addr >> 12) & 0x3FF;

   unsigned long* page_table = (unsigned long*)(current_page_table->page_directory[page_dir_index] & 0xFFFFF000);
   unsigned long* pte_addr = &(page_table[page_table_index]);

   return pte_addr;
}



void PageTable::register_pool(VMPool* _vm_pool) {

   //if nullptr, allocate
   if (vmpools == nullptr) {
      vmpools = new VMPool*[10];
      capacity = 10;
      vmpool_n = 0;  
   }
   if (vmpool_n >= capacity) { //if our array is full, resize
      capacity *= 2; 
      VMPool** newVmpools = new VMPool*[capacity];

      for (int i = 0; i < vmpool_n; ++i) {
         newVmpools[i] = vmpools[i];
      }

      delete[] vmpools;

      vmpools = newVmpools;
   }

   vmpools[vmpool_n++] = _vm_pool; //register new pool

   //  assert(false);
   Console::puts("registered VM pool\n");
}

void PageTable::free_page(unsigned long _page_no) {
   unsigned long * entry = PTE_address(_page_no);
   
   if (*entry & PAGE_PRESENT) {
      ContFramePool::release_frames(*entry); 

      *entry = *entry & 0xFFFFFFFE; //clear entry
    	write_cr3((unsigned long)page_directory);
   } 
   Console::puts("page freed\n");
}
