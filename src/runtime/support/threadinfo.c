#include "threadinfo.h"

#define PTR_IN_RANGE(V) ((MIN_ALLOCATED_ADDRESS <= V) & (V <= MAX_ALLOCATED_ADDRESS))
#define PTR_NOT_IN_STACK(BASE, CURR, V) ((V < CURR) | (BASE < V))

#define PROCESS_REGISTER(BASE, CURR, R) \
    register void* R asm(#R); \
    native_register_contents.##R = NULL;\
    if(PTR_IN_RANGE(R) & PTR_NOT_IN_STACK(BASE, CURR, R)) { native_register_contents.##R = R; }

void initializeStartup()
{
    mtx_init(&g_lock, mtx_plain);
}

void initializeThreadLocalInfo()
{
    int lckok = mtx_lock(&g_lock);
    assert(lckok == thrd_success);

    tl_id = tl_id_counter++;
    xallocInitializePageManager(tl_id);

    int unlckok = mtx_unlock(&g_lock);
    assert(unlckok == thrd_success);
    
    register void* rbp asm("rbp");   
    native_stack_base = rbp;
}

void loadNativeRootSet()
{
    native_stack_contents = (void**)xallocAllocatePage();
    xmem_pageclear(native_stack_contents);

    //this code should load from the asm stack pointers and copy the native stack into the roots memory
    #ifdef __x86_64__
        register void* rsp asm("rsp");
        void** current_frame = rsp;
        int i = 0;

        while (current_frame < native_stack_base) {
            void* potential_ptr = *(current_frame + 1);
            if (ALLOC_IN_RANGE(potential_ptr) & PTR_NOT_IN_STACK(native_stack_base, current_frame, potential_ptr)) {
                native_stack_contents[i++] = potential_ptr;
            }
            current_frame = *(void**)current_frame;
        }

        PROCESS_REGISTER(native_stack_base, current_frame, rax)
        PROCESS_REGISTER(native_stack_base, current_frame, rbx)
        PROCESS_REGISTER(native_stack_base, current_frame, rcx)
        PROCESS_REGISTER(native_stack_base, current_frame, rdx)
        PROCESS_REGISTER(native_stack_base, current_frame, rsi)
        PROCESS_REGISTER(native_stack_base, current_frame, rdi)
        PROCESS_REGISTER(native_stack_base, current_frame, r8)
        PROCESS_REGISTER(native_stack_base, current_frame, r9)
        PROCESS_REGISTER(native_stack_base, current_frame, r10)
        PROCESS_REGISTER(native_stack_base, current_frame, r11)
        PROCESS_REGISTER(native_stack_base, current_frame, r12)
        PROCESS_REGISTER(native_stack_base, current_frame, r13)
        PROCESS_REGISTER(native_stack_base, current_frame, r14)
        PROCESS_REGISTER(native_stack_base, current_frame, r15)
    #else
        #error "Architecture not supported"
    #endif
}

void unloadNativeRootSet()
{
    xallocFreePage(native_stack_contents);
}