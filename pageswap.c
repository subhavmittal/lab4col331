#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "buf.h"

struct slot{
    int page_perm;
    int is_free;
};
struct slot diskslots[NSWAPBLOCKS/8];

// Initialize the swap slots at boot time
void init_swap_slots(){
    for(int i=0; i<NSWAPBLOCKS/8; i++){
        diskslots[i].page_perm = 0;
        diskslots[i].is_free = 1;
    }
}

// write the page to the disk
void write_page_to_disk(char *va, int slot){
    // read data from the page into array of buffers
    struct buf *b;
    for (int i = 0; i < 8; i++){
        b = bread(ROOTDEV, (slot*8)+i+SWAPSTART);
        memmove(b->data, va+(i*512), 512);
        bwrite(b);
        brelse(b);
    }
}

int swap_out(){
    // find a victim process
    struct proc *p;
    p = find_victim_proc();
    // find a victim page
    pte_t *pte;
    pte = find_victim_page(p->pgdir, p->sz);
    // decrement rss of the process
    p->rss = p->rss - PGSIZE;
    // find a free swap slot
    for (int i = 0; i < NSWAPBLOCKS/8; i++){
        if(diskslots[i].is_free){
            diskslots[i].is_free = 0;
            // write the page to the swap slot
            char *va = P2V(PTE_ADDR(*pte));
            write_page_to_disk(va, i);
            // store the page permissions in the slot
            diskslots[i].page_perm = *pte;
            // update the page table entry
            *pte = (*pte & ~PTE_P);
            // add disk address to the page table entry
            *pte = (*pte & 0xFFF) | (i << 12);
            // free the page
            kfree(va);
            return 0;
        }
    }
    return -1;
}
