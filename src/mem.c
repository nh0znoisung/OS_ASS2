
#include "mem.h"
#include "stdlib.h"
#include "string.h"
#include <pthread.h>
#include <stdio.h>

static BYTE _ram[RAM_SIZE];

static struct {
	uint32_t proc;	// ID of process currently uses this page
	int index;	// Index of the page in the list of pages allocated
			// to the process.
	int next;	// The next page in the list. -1 if it is the last
			// page.
} _mem_stat [NUM_PAGES];

static pthread_mutex_t mem_lock;

void init_mem(void) {
	memset(_mem_stat, 0, sizeof(*_mem_stat) * NUM_PAGES);
	memset(_ram, 0, sizeof(BYTE) * RAM_SIZE);
	pthread_mutex_init(&mem_lock, NULL);
}

/* get offset of the virtual address */
static addr_t get_offset(addr_t addr) {
	return addr & ~((~0U) << OFFSET_LEN);
}

/* get the first layer index */
static addr_t get_first_lv(addr_t addr) {
	return addr >> (OFFSET_LEN + PAGE_LEN);
}

/* get the second layer index */
static addr_t get_second_lv(addr_t addr) {
	return (addr >> OFFSET_LEN) - (get_first_lv(addr) << PAGE_LEN);
}

/* Search for page table table from the a segment table */
static struct page_table_t * get_page_table(
		addr_t index, 	// Segment level index
		struct seg_table_t * seg_table) { // first level table
	
	/*
	 * TODO: Given the Segment index [index], you must go through each
	 * row of the segment table [seg_table] and check if the v_index
	 * field of the row is equal to the index
	 *
	 * */

	int i;
	for (i = 0; i < seg_table->size; i++) {
		// Enter your code here
		if (seg_table->table[i].v_index == index)
		{
			return seg_table->table[i].pages;
		}
	}
	return NULL;

}

/* Translate virtual address to physical address. If [virtual_addr] is valid,
 * return 1 and write its physical counterpart to [physical_addr].
 * Otherwise, return 0 */
static int translate(
		addr_t virtual_addr, 	// Given virtual address
		addr_t * physical_addr, // Physical address to be returned
		struct pcb_t * proc) {  // Process uses given virtual address

	/* Offset of the virtual address */
	addr_t offset = get_offset(virtual_addr);
	/* The first layer index */
	addr_t first_lv = get_first_lv(virtual_addr);
	//printf("SEGMENT INDEX: %d\n", first_lv);
	/* The second layer index */
	addr_t second_lv = get_second_lv(virtual_addr);
	//printf("SECOND INDEX: %d\n", second_lv);
	
	
	/* Search in the first level */
	struct page_table_t * page_table = NULL;
	page_table = get_page_table(first_lv, proc->seg_table);
	if (page_table == NULL) {
		return 0;
	}

	int i;
	for (i = 0; i < page_table->size; i++) {
		if (page_table->table[i].v_index == second_lv) {
			/* TODO: Concatenate the offset of the virtual addess
			 * to [p_index] field of page_table->table[i] to 
			 * produce the correct physical address and save it to
			 * [*physical_addr]  */
			*physical_addr = (page_table->table[i].p_index << OFFSET_LEN) + offset;
			//printf("Physical index: %d\n", page_table->table[i].p_index);
			//printf("OFFSET: %d\n", *physical_addr);
			return 1;
		}
	}
	return 0;	
}

addr_t alloc_mem(uint32_t size, struct pcb_t * proc) {
	pthread_mutex_lock(&mem_lock);
	addr_t ret_mem = 0;
	/* TODO: Allocate [size] byte in the memory for the
	 * process [proc] and save the address of the first
	 * byte in the allocated memory region to [ret_mem].
	 * */

	uint32_t num_pages = (size % PAGE_SIZE == 0) ? size / PAGE_SIZE :
		size / PAGE_SIZE + 1; // Number of pages we will use
	int mem_avail = 0; // We could allocate new memory region or not?

	/* First we must check if the amount of free memory in
	 * virtual address space and physical address space is
	 * large enough to represent the amount of required 
	 * memory. If so, set 1 to [mem_avail].
	 * Hint: check [proc] bit in each page of _mem_stat
	 * to know whether this page has been used by a process.
	 * For virtual memory space, check bp (break pointer).
	 * */

	//traverse the _mem_stat list and look for free pages
	uint32_t free_pages = 0;
	for (int i = 0; i < NUM_PAGES; i++)
	{
		if (_mem_stat[i].proc == 0) free_pages++;
		//if there are enough free pages, we conclue that we can allocate the required amount
		if (free_pages == num_pages)
		{
			mem_avail = 1;
			break;
		}
	}

	//check for availability in the virtual memory space
	//if the bp exceeds the size of virtual RAM (which is 1MB) --> new memory can't be allocated
	//0xfffff is the largest 20-bit number, so it is the max value bp can take.
	if (proc->bp + num_pages * PAGE_SIZE > 0xfffff) mem_avail = 0;
	
	if (mem_avail) {
		/* We could allocate new memory region to the process */
		ret_mem = proc->bp;
		proc->bp += num_pages * PAGE_SIZE;
		/* Update status of physical pages which will be allocated
		 * to [proc] in _mem_stat. Tasks to do:
		 * 	- Update [proc], [index], and [next] field
		 * 	- Add entries to segment table page tables of [proc]
		 * 	  to ensure accesses to allocated memory slot is
		 * 	  valid. */


		//store the value of the latest allocated page, used for updating the "next" field in _mem_stat
		int latest_page = -1;
		//store the index of the current alocated page, used for updating the "index" field in _mem_stat
		int index_count = 0;
		int needed_pages = num_pages;
		int first_page = 0;
		for (int i = 0; i < NUM_PAGES; i++)
		{
			//looking for free pages
			if (_mem_stat[i].proc == 0)
			{
				needed_pages--; 
				_mem_stat[i].proc = proc->pid;
				_mem_stat[i].index = index_count;
				if (latest_page != -1) _mem_stat[latest_page].next = i;
				else first_page = i;
				index_count++;
				latest_page = i;
			}
			if (needed_pages == 0) 
			{
				//update the "next" field of the final page
				_mem_stat[latest_page].next = -1;
				break;
			}
		}
		//add a new entry to the segment table
		struct seg_table_t* seg_table = proc->seg_table;
		int valid_index;
		int duplicate_index;
		//find a valid virtual index for the next row
		for (valid_index = 0; valid_index < ~(~0 << SEGMENT_LEN) + 1; valid_index++)
		{
			duplicate_index = 0;
			for(int i = 0; i < seg_table->size; i++)
			{
				if (valid_index == seg_table->table[i].v_index)
				{
					duplicate_index = 1;
					break;
				}
			}
			if (!duplicate_index) break;
		}
		//add a new row to the segment table with the newly found index
		//printf("INDEX: %d\t%d\n", valid_index, get_first_lv(ret_mem));
		seg_table->size++;
		seg_table->table[seg_table->size - 1].v_index = get_first_lv(ret_mem);
		seg_table->table[seg_table->size - 1].pages = (struct page_table_t* )malloc(sizeof(struct page_table_t));
		//puts("HELLO");
		//build the page table
		struct page_table_t *page_table = seg_table->table[seg_table->size - 1].pages;
		int physical_index = first_page;
		page_table->size = num_pages;
		for (int i = get_second_lv(ret_mem); i < num_pages; i++)
		{
			page_table->table[i].v_index = i;
			page_table->table[i].p_index = physical_index;
			physical_index = _mem_stat[i].next;	//update the index of the next page in physical memory
		}



	}

	pthread_mutex_unlock(&mem_lock);
	//printf("ALLOC: %d\n", ret_mem);
	return ret_mem;
}

int free_mem(addr_t address, struct pcb_t * proc) {
	/*TODO: Release memory region allocated by [proc]. The first byte of
	 * this region is indicated by [address]. Task to do:
	 * 	- Set flag [proc] of physical page use by the memory block
	 * 	  back to zero to indicate that it is free.
	 * 	- Remove unused entries in segment table and page tables of
	 * 	  the process [proc].
	 * 	- Remember to use lock to protect the memory from other
	 * 	  processes.  */
	pthread_mutex_lock(&mem_lock);
	
	//get page table, where proc in
	struct page_table_t * page_table = get_page_table(get_first_lv(address), proc->seg_table);

	int valid = 0;	//check success
	if(page_table != NULL){
		int i;

		//traverse page table
		for(i = 0; i < page_table->size; i++){

			//page need to find
			if(page_table->table[i].v_index == get_second_lv(address)){
				addr_t physical_addr;
				if(translate(address, &physical_addr, proc)){
					int p_index = physical_addr >> OFFSET_LEN;	//page index on memory
					addr_t seg_idx,page_idx;	//segment index and page index of current region
					do{
						//set flag [proc] of physical page back to 0 => memory block was free
						_mem_stat[p_index].proc = 0;
						
						int found = 0;
						seg_idx=get_first_lv(address);
						page_idx=get_second_lv(address);

						int k;
						for(k = 0; k < proc->seg_table->size && !found; k++){
							if( proc->seg_table->table[k].v_index == seg_idx ){
								int l;
								for(l = 0; l < proc->seg_table->table[k].pages->size; l++){
									if(proc->seg_table->table[k].pages->table[l].v_index== page_idx){
										int m;
										for(m = l; m < proc->seg_table->table[k].pages->size - 1; m++)//Rearrange page table
											proc->seg_table->table[k].pages->table[m]= proc->seg_table->table[k].pages->table[m + 1];
										
										proc->seg_table->table[k].pages->size--;
										if(proc->seg_table->table[k].pages->size == 0){//If all pages in segment are free
											free(proc->seg_table->table[k].pages);
											for(m = k; m < proc->seg_table->size - 1; m++)//Rearrange segment table
												proc->seg_table->table[m]= proc->seg_table->table[m + 1];
											proc->seg_table->size--;
										}
										found = 1;
										break;
									}
								}
							}
						}
						
						//go to the next page of proc
						p_index = _mem_stat[p_index].next;
					}
					while(p_index != -1);
					valid = 1;
				}
				break;
			}
		}
	}

	pthread_mutex_unlock(&mem_lock);

	if(!valid)
		return 1;
	return 0;
}
int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		*data = _ram[physical_addr];
		return 0;
	}else{
		return 1;
	}
}

int write_mem(addr_t address, struct pcb_t * proc, BYTE data) {
	addr_t physical_addr;
	if (translate(address, &physical_addr, proc)) {
		_ram[physical_addr] = data;
		//printf("WRITE: %d\n", physical_addr);
		return 0;
	}else{
		//puts("TRANSLATE FAILED");
		return 1;
	}
}

void dump(void) {
	int i;
	for (i = 0; i < NUM_PAGES; i++) {
		if (_mem_stat[i].proc != 0) {
			printf("%03d: ", i);
			printf("%05x-%05x - PID: %02d (idx %03d, nxt: %03d)\n",
				i << OFFSET_LEN,
				((i + 1) << OFFSET_LEN) - 1,
				_mem_stat[i].proc,
				_mem_stat[i].index,
				_mem_stat[i].next
			);
			int j;
			for (	j = i << OFFSET_LEN;
				j < ((i+1) << OFFSET_LEN) - 1;
				j++) {
				
				if (_ram[j] != 0) {
					printf("\t%05x: %02x\n", j, _ram[j]);
				}
					
			}
		}
	}
}


