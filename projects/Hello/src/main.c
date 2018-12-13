#include <stdio.h>
#include <sel4platsupport/bootinfo.h>
#include <simple-default/simple-default.h>

#include <allocman/allocman.h>
#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <sel4utils/vspace.h>


#include <sel4utils/thread.h>
#include <sel4utils/thread_config.h>

/* global environment variables */
seL4_BootInfo *info;
simple_t simple;
vka_t vka;
allocman_t *allocman;
vspace_t vspace;


/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE (BIT(seL4_PageBits) * 10)
UNUSED static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE (BIT(seL4_PageBits) * 100)

/* static memory for virtual memory bootstrapping */
UNUSED static sel4utils_alloc_data_t data;

/* stack for the new thread */
#define THREAD_2_STACK_SIZE 4096
UNUSED static int thread_2_stack[THREAD_2_STACK_SIZE];



void threadStart(void *arg0, void *arg1, void *ipc_buf)
{

    printf("Thread is running !\n");

    while(1)
    {
	
    }

}


int main(void)
{
    UNUSED int error = 0;

    /* get boot info */
    info = platsupport_get_bootinfo();
    ZF_LOGF_IF(info == NULL, "Failed to get bootinfo.");

    /* Set up logging and give us a name: useful for debugging if the thread faults */
    zf_log_set_tag_prefix("init:");
    //name_thread(seL4_CapInitThreadTCB, "hello-4");

    /* init simple */
    simple_default_init_bootinfo(&simple, info);

    /* create an allocator */
    allocman = bootstrap_use_current_simple(&simple, ALLOCATOR_STATIC_POOL_SIZE,
                                            allocator_mem_pool);
    ZF_LOGF_IF(allocman == NULL, "Failed to initialize allocator.\n"
               "\tMemory pool sufficiently sized?\n"
               "\tMemory pool pointer valid?\n");

    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&vka, allocman);


    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky( &vspace , &data ,simple_get_pd(&simple)  ,&vka, info); 
    ZF_LOGF_IFERR(error, "Failed to prepare root thread's VSpace for use.\n"
                  "\tsel4utils_bootstrap_vspace_with_bootinfo reserves important vaddresses.\n"
                  "\tIts failure means we can't safely use our vaddrspace.\n");

    assert(error == 0);

    /* fill the allocator with virtual memory */
    void *vaddr;
    UNUSED reservation_t virtual_reservation;
    virtual_reservation = vspace_reserve_range(&vspace,
                                               ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
    ZF_LOGF_IF(virtual_reservation.res == NULL, "Failed to reserve a chunk of memory.\n");
    bootstrap_configure_virtual_pool(allocman, vaddr,
                                     ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&simple));


    sel4utils_thread_config_t threadConf = thread_config_new(&simple);

    sel4utils_thread_t thread;

    error = sel4utils_configure_thread_config(&vka , &vspace , /*alloc*/&vspace , threadConf , &thread);
    assert(error == 0);
    ZF_LOGF_IFERR(error, "Failed to sel4utils_configure_thread_config\n");

    seL4_TCB_SetPriority(thread.tcb.cptr, seL4_CapInitThreadTCB ,  255);

    error = sel4utils_start_thread(&thread , threadStart , NULL , NULL , 1);

    assert(error == 0);
    ZF_LOGF_IFERR(error, "Failed  to start thread\n");

    printf("So far so good\n");

    while(1){}

    return 0;
}

