#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

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

