#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"


struct slot{
    int page_perm;
    int is_free;
};
struct slot diskslots[NSWAPBLOCKS/8];

pte_t *
walkpgdir2(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

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
        // cprintf("block no %d\n", (slot*8)+i+SWAPSTART);
        b = bread(ROOTDEV, (slot*8)+i+SWAPSTART);
        // cprintf("2f\n");
        memmove(b->data, va+(i*512), 512);
        bwrite(b);
        brelse(b);
    }
}

char* swap_out(){
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
            // reacquire kernel lock
            // acquire(&kmem.lock);
            // cprintf("2f\n");
            // store the page permissions in the slot
            diskslots[i].page_perm = *pte;
            // update the page table entry
            *pte = (*pte & ~PTE_P);
            *pte = (*pte | PTE_PG);
            // add disk address to the page table entry
            *pte = ((uint)*pte & 0xFFF) | (i << 12);
            // free the page
            return va;
        }
    }
    // cprintf("No free swap slots\n");
    return (char*)0;
}

void
swap_in(uint va)
{
    // Implement the swapping-in procedure
    struct proc *curproc = myproc();
    pte_t *pte = walkpgdir2(curproc->pgdir, (void *)va, 0);
    //update rss of the process
    curproc->rss = curproc->rss + PGSIZE;

    if (!pte || !(*pte & PTE_PG)) {
        return;
    }

    int snum = PTE_ADDR(*pte) >> 12;
    // cprintf("swap in\n");
    // cprintf("slot %d\n", snum);
    struct slot *s = &diskslots[snum];

    if (s->is_free) {
        return;
    }

    char *mem = kalloc();
    // cprintf("mem aa gya");
    struct buf *b;
    for (int i = 0; i < 8; i++) {
        b = bread(ROOTDEV, (snum*8)+i+SWAPSTART);
        memmove(mem+(i*512), b->data, 512);
        brelse(b);
    }
    //get permissions from the slot
    *pte = s->page_perm;
    *pte = V2P(mem) | PTE_FLAGS(*pte);
    //Set the page as present and accessed
    *pte = (*pte | PTE_P);
    *pte = (*pte | PTE_A);
    s->is_free = 1;
    s->page_perm = 0;
    // lcr3(V2P(curproc->pgdir));
    return;
}

void free_slot(int slot){
    diskslots[slot].is_free = 1;
    // cprintf("slot %d is free\n", slot);
}