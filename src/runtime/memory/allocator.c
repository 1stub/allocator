#include "allocator.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CANARY_DEBUG_CHECK

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .evacuate_page = NULL, .filled_pages = NULL};

static void setup_freelist(PageInfo* pinfo, uint16_t entrysize) {
    FreeListEntry* current = pinfo->freelist;

    for(int i = 0; i < pinfo->entrycount - 1; i++) {
        current->next = (FreeListEntry*)((char*)current + REAL_ENTRY_SIZE(entrysize));
        current = current->next;
    }
    current->next = NULL;
}

static PageInfo* initializePage(void* page, uint16_t entrysize)
{
    debug_print("New page!\n");

    PageInfo* pinfo = (PageInfo*)page;
    pinfo->freelist = (FreeListEntry*)((char*)page + sizeof(PageInfo));
    pinfo->entrysize = entrysize;
    pinfo->entrycount = (BSQ_BLOCK_ALLOCATION_SIZE - sizeof(PageInfo)) / REAL_ENTRY_SIZE(entrysize);
    pinfo->freecount = pinfo->entrycount;
    pinfo->pagestate = PageStateInfo_GroundState;

    setup_freelist(pinfo, pinfo->entrysize);

    return pinfo;
}

/**
* Whenever this method is called we need to insert address into our hierarchical page manager so its easy to 
* see if said page exists.
**/
PageInfo* allocateFreshPage(uint16_t entrysize)
{
#ifdef ALLOC_DEBUG_MEM_DETERMINISTIC
    static void* old_base = GC_ALLOC_BASE_ADDRESS;
    void* page = mmap((void*)old_base, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    old_base += BSQ_BLOCK_ALLOCATION_SIZE;
#else
    void* page = mmap(NULL, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
#endif

    assert(page != MAP_FAILED);

    if(!pagetable_root) {
        pagetable_init();
    }
    pagetable_insert(page);

    return initializePage(page, entrysize);
}

void getFreshPageForAllocator(AllocatorBin* alloc)
{
    if(alloc->page != NULL) {
        //need to rotate our old page into the collector, now alloc->page
        //exists in the needs_collection list
        alloc->page->pagestate = AllocPageInfo_ActiveEvacuation;
        alloc->page->next = alloc->page_manager->filled_pages;
        alloc->page_manager->filled_pages = alloc->page;
    }
    PageInfo* new_page = allocateFreshPage(alloc->entrysize);

    // Add new page to all pages list
    new_page->next = alloc->page_manager->all_pages;
    alloc->page_manager->all_pages = new_page;

    // Make sure the current alloc page points to this entry in the list
    alloc->page = new_page;
    alloc->freelist = new_page->freelist;
}

AllocatorBin* initializeAllocatorBin(uint16_t entrysize)
{
    AllocatorBin* bin = &a_bin;
    if(bin == NULL) return NULL;

    bin->page_manager = &p_mgr;
    bin->page_manager->evacuate_page = allocateFreshPage(DEFAULT_ENTRY_SIZE);

    getFreshPageForAllocator(bin);

    return bin;
}