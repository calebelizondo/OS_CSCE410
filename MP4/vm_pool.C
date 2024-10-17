/*
 File: vm_pool.C
 
 Author: Caleb Elizondo
 Date  : 10/17/24
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long base_address, unsigned long size, ContFramePool* frame_pool, PageTable* page_table)
    : pool_base_address(base_address),
      pool_size(size),
      frame_pool(frame_pool),
      page_table(page_table),
      allocated_count(0),
      free_count(0),
      is_first_entry_initialized(false) {
    
    //needs to be addressable w 12 bits!
    assert(pool_size < 2^12);

    page_table->register_pool(this);

    //allocate allocated and free lists
    allocated_regions = reinterpret_cast<MemoryRegion*>(pool_base_address);
    free_regions = reinterpret_cast<MemoryRegion*>(allocated_regions + (2^8));

    //manually establish first entries
    is_first_entry_initialized = true;
    allocated_regions[0] = { pool_base_address, 2^12 };
    free_regions[0] = { pool_base_address + 2^12, pool_size - 2^12 };
    is_first_entry_initialized = false;
    allocated_count++;
    free_count++;

    //init remaining to default values
    for (int i = 1; i < 2^8; ++i) {
        allocated_regions[i] = { 0, 0 };
        free_regions[i] = { 0, 0 };
    }

    Console::puts("Constructed VMPool object.\n");
}

unsigned long VMPool::allocate(unsigned long size) {
    unsigned long free_index = 0;
    bool found = false;
    //find a region that fits
    for (unsigned long i = 0; i < free_count; ++i) {
        if (size <= free_regions[i].size) {
            free_index = i;
            found = true;
            break;
        }
    }
    //if found, allocate the region ...
    if (found) {
        allocated_regions[allocated_count] = { free_regions[free_index].base_address, size };
        allocated_count++;

        free_regions[free_index].base_address += size;
        free_regions[free_index].size -= size;

        Console::puts("Allocated memory region.\n");
        return allocated_regions[allocated_count - 1].base_address;
    } else { //otherwise, simply return 0
        Console::puts("Not enough memory for allocation.\n");
        return 0;
    }
}

void VMPool::release(unsigned long start_address) {
    unsigned long allocated_index = 0;
    bool found = false;
    //find the memory region ...
    for (unsigned long i = 0; i < allocated_count; ++i) {
        if (allocated_regions[i].base_address == start_address) {
            allocated_index = i;
            found = true;
            break;
        }
    }


    //if the region is not found, throw an error
    if (!found) {
        Console::puts("Memory region not found.\n");
        assert(false);
    }

    if (allocated_regions[allocated_index].size == 0) {
        Console::puts("Invalid allocated memory region.\n");
        assert(false);
    }

    free_regions[free_count] = { allocated_regions[allocated_index].base_address, allocated_regions[allocated_index].size };
    free_count++;

    allocated_regions[allocated_index] = { 0, 0 };

    page_table->free_page(start_address);

    Console::puts("Released memory region.\n");
}

bool VMPool::is_legitimate(unsigned long address) {

    if (is_first_entry_initialized && allocated_count == 0) {
        return true;
    }
    //find valid address ...
    for (unsigned long i = 0; i < allocated_count; ++i) {
        if (allocated_regions[i].base_address <= address && address < allocated_regions[i].base_address + allocated_regions[i].size) {
            return true;
        }
    }

    return false;
}
