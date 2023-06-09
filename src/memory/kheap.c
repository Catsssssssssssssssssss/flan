#include "lib/stdio.h"
#include <stddef.h>
#include  "memory/kheap.h"
#include "lib/str.h"
#include  "lib/lock.h"
#include "memory/vmm.h"

#define HEADER_SIZE 24
static dll_t free_list={.next=&free_list,.prev=&free_list};
spinlock_t kheap_lock=LOCK_INIT;

void dll_list_add(dll_t* n, dll_t* prev, dll_t* next){
	next->prev = n;
	n->next = next;
	n->prev = prev;
	prev->next = n;
}

void add_block(uint64_t *addr, uint64_t size){
	alloc_node_t* block = addr;
	block->size = size -  HEADER_SIZE;
	dll_list_add(&block->node,free_list.prev,&free_list);
};

void coalesce_dll(){
	alloc_node_t *prevBlock;
	for (alloc_node_t* block=container_of(free_list.next,alloc_node_t,node); &block->node!= &free_list; block=container_of(block->node.next,alloc_node_t,node)){
		if ((uint64_t*)((char*)prevBlock+prevBlock->size+HEADER_SIZE)==block)
		{
			prevBlock->size+=block->size+HEADER_SIZE;
			//removes block
			block->node.next->prev =block->node.prev;
			block->node.prev->next=block->node.next;
			continue;
		}
		prevBlock=block;
	}
}

void* kheap_alloc(uint64_t size){
	spinlock_acquire(&kheap_lock);
	void* ptr=NULL;
	alloc_node_t* block;
	// Try to find a big enough block to alloc (First fit)
	for (block = container_of(free_list.next,alloc_node_t,node); &block->node != &free_list; block=container_of(block->node.next,alloc_node_t,node))
	{      
		if (block->size>=size)
		{   
			ptr = &block->cBlock;
			//printf("\nFound block for requested size at: %x\n",ptr);
			break;
		}        
	}
	if (!ptr)
	{
        uint64_t frames = (size/FRAME_SIZE) + (size % FRAME_SIZE);
		uint64_t* new =pmm_malloc(frames);
		add_block(new,frames * FRAME_SIZE);
		coalesce_dll();
		spinlock_release(&kheap_lock);
		return kheap_alloc(size);
	}

	//Can block be split
	if((block->size - size) >= HEADER_SIZE)
	{
		alloc_node_t *new_block = (alloc_node_t *)((uint64_t*)((char*)&block->cBlock + size));
		new_block->size = block->size - size - HEADER_SIZE;
		block->size =  size;
		//add new block to list
		dll_list_add(&new_block->node,&block->node,block->node.next);
	}
	//remove block from list since its getting allocated 
	block->node.next->prev =block->node.prev;
	block->node.prev->next=block->node.next;
	spinlock_release(&kheap_lock);
	//Finally, return pointer to newly allocated adress
	return ptr+HHDM_OFFSET;  
}

void* kheap_calloc(uint64_t size){
	void* ptr = kheap_alloc(size);
	if (ptr!=NULL)
	{
		memset(ptr,0,size);
		return ptr;
	}
	return NULL;
}

void kheap_free(uint64_t ptr){
	alloc_node_t *block, *free_block;
	block = (void*) container_of(ptr, alloc_node_t,cBlock) - HHDM_OFFSET;
    if ((block->size+sizeof(alloc_node_t)) >= FRAME_SIZE && ((uint64_t) container_of(block, alloc_node_t, cBlock) % FRAME_SIZE) == 0) {
        pmm_free(container_of(block, alloc_node_t, cBlock), (block->size+sizeof(alloc_node_t)) / FRAME_SIZE);                
	    return;
    }

	spinlock_acquire(&kheap_lock);
	for (free_block = container_of(free_list.next,alloc_node_t,node); &free_block->node!= &free_list; free_block=container_of(free_block->node.next,alloc_node_t,node))
	{
		if (free_block>block)
		{
			dll_list_add(&block->node,free_block->node.prev,&free_block->node);
			coalesce_dll(); //prevent fragmentation 
			spinlock_release(&kheap_lock);
			return;
		}
	}
	dll_list_add(&block->node,&free_block->node,free_block->node.next);

	coalesce_dll();//prevent fragmentation 
	spinlock_release(&kheap_lock);
	return;
}
void* kheap_realloc(void *ptr, uint64_t new_size){
	if (!ptr && !new_size) {
		return NULL;
	}
	if (!new_size) {
		kheap_free(ptr);
		return NULL;
	}
	if (!ptr) {
		return kheap_alloc(new_size);
	}
    alloc_node_t* node=container_of(ptr,alloc_node_t,cBlock);
    uint64_t old_size=node->size;
	void *ret = kheap_alloc(new_size);
	if (!ret) {
		return NULL;
	}
	memcpy(ret, ptr, old_size);
	return ret;
}

void kheap_init(){
	add_block(pmm_malloc(1),FRAME_SIZE);
    printf("Kernel heap initialized.\n");
}
