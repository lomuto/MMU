#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define PFN_MASK 0b11111100
#define PG_DIR_MASK 0b11000000
#define PG_MIDDLE_DIR_MASK 0b00110000
#define PG_TABLE_MASK 0b00001100
#define UNMAPPED 0b00000000
#define PRESENT_MASK 0b00000001
#define PAGE_SIZE 4
#define SWAP_MEM(x, y)   \
    {                    \
        (x) = (x) ^ (y); \
        (y) = (x) ^ (y); \
        (x) = (x) ^ (y); \
    }
typedef char ku_pte; // 1byte
typedef int PAGE_t;

/*****************************************************************
*                     Process Control Block                      *
******************************************************************/

typedef struct _PCB {
    PAGE_t *pdbr;
    char pid;
} PCB;

typedef struct _PCB_NODE {
    PCB *data;
    struct _PCB_NODE *NEXT;
} PCB_NODE;

PCB_NODE *ku_mmu_HEAD_pcblist = NULL;

void INIT_pcblist() {
    ku_mmu_HEAD_pcblist = (PCB_NODE *)malloc(sizeof(PCB_NODE));
    ku_mmu_HEAD_pcblist->data = NULL;
    ku_mmu_HEAD_pcblist->NEXT = NULL;
}

PCB *CREATE_pcb(char pid, void *pdbr) {
    PCB *new_pcb = malloc(sizeof(PCB));
    new_pcb->pid = pid;
    new_pcb->pdbr = pdbr;

    return new_pcb;
}

bool is_pcblist_empty() {
    return ku_mmu_HEAD_pcblist->NEXT == NULL ? true : false;
}

void ADD_pcblist(PCB *new_pcb) {
    PCB_NODE *new_pcb_node = (PCB_NODE *)malloc(sizeof(PCB_NODE));
    new_pcb_node->data = new_pcb;
    new_pcb_node->NEXT = NULL;

    if (is_pcblist_empty()) {
        ku_mmu_HEAD_pcblist = (PCB_NODE *)malloc(sizeof(PCB_NODE));
        ku_mmu_HEAD_pcblist->NEXT = new_pcb_node;
    } else {
        PCB_NODE *temp = ku_mmu_HEAD_pcblist->NEXT;
        ku_mmu_HEAD_pcblist->NEXT = new_pcb_node;
        new_pcb_node->NEXT = temp;
    }
}

PCB *SEARCH_pcblist(char pid) {
    for (PCB_NODE *ptr = ku_mmu_HEAD_pcblist->NEXT; ptr != NULL; ptr = ptr->NEXT)
        if (ptr->data->pid == pid)
            return ptr->data;
    return NULL;
}

/*****************************************************************
*                         Reusable NODE                          *
******************************************************************/
typedef struct _NODE {
    ku_pte *ADDR;
    struct _NODE *NEXT;
    struct _NODE *PREV;
} NODE;

/*****************************************************************
*                    Physical Memory Freelist                    *
******************************************************************/

void *ku_mmu_pmem_entry;

NODE *ku_mmu_HEAD_pmemlist = NULL;

void INIT_pmemlist(int mem_size) {
    ku_mmu_HEAD_pmemlist = (NODE *)malloc(sizeof(NODE));
    ku_mmu_HEAD_pmemlist->ADDR = NULL;

    int total_seg = mem_size / 4;
    NODE *ptr = ku_mmu_HEAD_pmemlist;

    for (int i = 1; i <= total_seg; i++) {
        NODE *temp = (NODE *)malloc(sizeof(NODE));
        temp->ADDR = ku_mmu_pmem_entry + PAGE_SIZE * i;
        ptr->NEXT = temp;
        ptr = ptr->NEXT;
    }

    ptr->NEXT = NULL;
}

bool is_pmemlist_empty() {
    return ku_mmu_HEAD_pmemlist->NEXT == NULL ? true : false;
}

// ISSUE
/*
* THIS VERSION OF DEQUEUE LIST WOULD YIELD AN UNEXPECTED BEHAVIOR: ADDRESS OF NODE GET SHIFTED
* ```
* Address: 0x5626d40ef264
* Address: 0x5626d40ef268
* Address: 0x5626d40ef2e0
* ```
* LAST ONE SHOULD BE `0x5626d40ef26c`
*/
// void *DEQUEUE_pmemlist() {
//     if (is_pmemlist_empty())
//         return NULL;

//     NODE *temp = ku_mmu_HEAD_pmemlist->NEXT;
//     void **ret;
//     *ret = temp->addr;
//     ku_mmu_HEAD_pmemlist->NEXT = ku_mmu_HEAD_pmemlist->NEXT->NEXT;
//     free(temp);

//     return *ret;
// }

int DEQUEUE_pmemlist(PAGE_t **arg) {
    if (is_pmemlist_empty())
        return 0;

    NODE *temp = ku_mmu_HEAD_pmemlist->NEXT;
    *arg = (PAGE_t *)temp->ADDR;
    ku_mmu_HEAD_pmemlist->NEXT = ku_mmu_HEAD_pmemlist->NEXT->NEXT;

    return 1;
}

/*****************************************************************
*                    Swap Space Freelist                         *
******************************************************************/

void *ku_mmu_swap_entry;

NODE *ku_mmu_HEAD_swaplist = NULL;

void INIT_swaplist(int mem_size) {
    ku_mmu_HEAD_swaplist = (NODE *)malloc(sizeof(NODE));
    ku_mmu_HEAD_swaplist->ADDR = NULL;

    int total_seg = mem_size / 4;
    NODE *ptr = ku_mmu_HEAD_swaplist;

    for (int i = 1; i <= total_seg; i++) {
        NODE *temp = (NODE *)malloc(sizeof(NODE));
        temp->ADDR = ku_mmu_swap_entry + PAGE_SIZE * i;
        ptr->NEXT = temp;
        ptr = ptr->NEXT;
    }

    ptr->NEXT = NULL;
}

bool is_swaplist_empty() {
    return ku_mmu_HEAD_swaplist->NEXT == NULL ? true : false;
}

int DEQUEUE_swaplist(PAGE_t **arg) {
    if (is_swaplist_empty())
        return 0;

    NODE *temp = ku_mmu_HEAD_swaplist->NEXT;
    *arg = (PAGE_t *)temp->ADDR;
    ku_mmu_HEAD_swaplist->NEXT = ku_mmu_HEAD_swaplist->NEXT->NEXT;

    return 1;
}

/****************************************************************
*                          Page List                          *
*****************************************************************/

NODE *ku_mmu_HEAD_pagelist = NULL;
NODE *ku_mmu_TAIL_pagelist = NULL;

void INIT_pagelist() {
    ku_mmu_HEAD_pagelist = (NODE *)malloc(sizeof(NODE));
    ku_mmu_TAIL_pagelist = (NODE *)malloc(sizeof(NODE));
    ku_mmu_HEAD_pagelist->NEXT = ku_mmu_TAIL_pagelist;
    ku_mmu_HEAD_pagelist->ADDR = NULL;
    ku_mmu_TAIL_pagelist->PREV = ku_mmu_HEAD_pagelist;
    ku_mmu_TAIL_pagelist->ADDR = NULL;
}

bool is_pagelist_empty() {
    return ku_mmu_HEAD_pagelist->NEXT == ku_mmu_TAIL_pagelist ? true : false;
}

void ENQUEUE_pagelist(ku_pte *page) {
    NODE *new_node = (NODE *)malloc(sizeof(NODE));
    new_node->ADDR = page;

    if (is_pagelist_empty()) {
        ku_mmu_HEAD_pagelist->NEXT = new_node;
        ku_mmu_TAIL_pagelist->PREV = new_node;
        new_node->NEXT = ku_mmu_TAIL_pagelist;
        new_node->PREV = ku_mmu_HEAD_pagelist;
        return;
    }

    NODE *temp = ku_mmu_HEAD_pagelist->NEXT;
    ku_mmu_HEAD_pagelist->NEXT = new_node;
    temp->NEXT->PREV = new_node;
    new_node->PREV = ku_mmu_HEAD_pagelist;
    new_node->NEXT = temp;
}

int DEQUEUE_pagelist(ku_pte **arg) {
    if (is_pagelist_empty())
        return 0;

    NODE *temp = ku_mmu_TAIL_pagelist->PREV;
    *arg = temp->ADDR;

    ku_mmu_TAIL_pagelist->PREV = temp->PREV;
    temp->PREV->NEXT = ku_mmu_TAIL_pagelist;

    return 1;
}

/*****************************************************************
*                        Initialize Entry                        *
******************************************************************/

void *ku_mmu_init(unsigned int pmem_size, unsigned int swap_size) {
    ku_mmu_pmem_entry = calloc(pmem_size, sizeof(ku_pte));
    if (ku_mmu_pmem_entry == NULL)
        return 0;

    ku_mmu_swap_entry = calloc(swap_size, sizeof(ku_pte));
    if (ku_mmu_swap_entry == NULL)
        return 0;

    INIT_pagelist();
    INIT_pmemlist(pmem_size);
    INIT_swaplist(swap_size);
    INIT_pcblist();

    return ku_mmu_pmem_entry;
}

/*****************************************************************
*                            Swap Out                            *
******************************************************************/

int SWAP_OUT(PAGE_t **PAGE) {
    PAGE_t *SWAP;
    ku_pte *ENTRY;

    // swap_space is full
    if (DEQUEUE_swaplist(&SWAP) == 0)
        return 0;
    // mapped segment doesn't exist (Error in initialization)
    if (DEQUEUE_pagelist(&ENTRY) == 0)
        return 0;

    int PFN = (*ENTRY & PFN_MASK) >> 2;
    *PAGE = ku_mmu_pmem_entry + PFN * PAGE_SIZE;

    SWAP_MEM(*SWAP, **PAGE);

    ku_pte SWAP_OFFSET = ((void *)SWAP - ku_mmu_swap_entry) / PAGE_SIZE;
    *ENTRY = SWAP_OFFSET << 1;

    return 1;
}

/*****************************************************************
*                       Create Page Table                        *
******************************************************************/

int ku_page_fault(char pid, char va) {
    PCB *curr_proc = SEARCH_pcblist(pid);

    // ERROR
    if (curr_proc == NULL && curr_proc->pdbr == NULL)
        return -1;

    int PG_DIR_OFFSET = (va & PG_DIR_MASK) >> 6;
    ku_pte *PG_DIR = (ku_pte *)curr_proc->pdbr + PG_DIR_OFFSET;

    /*
    *   Page fault in Middle Directory
    *   Create middle page
    */
    if (*PG_DIR == UNMAPPED) {
        PAGE_t *new_PG_MIDDLE_DIR;
        // pmem is full
        if (DEQUEUE_pmemlist(&new_PG_MIDDLE_DIR) == 0)
            if (SWAP_OUT(&new_PG_MIDDLE_DIR) == 0)
                return -1;

        *PG_DIR = ((((void *)new_PG_MIDDLE_DIR - ku_mmu_pmem_entry) / PAGE_SIZE) << 2) | PRESENT_MASK;
    } else if (*PG_DIR & PRESENT_MASK == 0)
        return -1;

    /*
    *   So far, exisitence of middle page is assuranced.
    */
    int PG_MIDDLE_DIR_OFFSET = (va & PG_MIDDLE_DIR_MASK) >> 4;
    ku_pte *PG_MIDDLE_DIR = ku_mmu_pmem_entry + (*PG_DIR >> 2) * PAGE_SIZE + PG_MIDDLE_DIR_OFFSET;

    /*
    *   Page fault in Page Table
    *   Create Page Table 
    */
    if (*PG_MIDDLE_DIR == UNMAPPED) {
        PAGE_t *new_PG_TABLE;
        // pmem is full
        if (DEQUEUE_pmemlist(&new_PG_TABLE) == 0)
            if (SWAP_OUT(&new_PG_TABLE) == 0)
                return -1;

        *PG_MIDDLE_DIR = ((((void *)new_PG_TABLE - ku_mmu_pmem_entry) / PAGE_SIZE) << 2) | PRESENT_MASK;
    } else if (*PG_MIDDLE_DIR & PRESENT_MASK == 0)
        return -1;

    /*
    *   So far, exisitence of page table is assuranced.
    */

    int PG_TABLE_OFFSET = (va & PG_TABLE_MASK) >> 2;
    ku_pte *PG_TABLE = ku_mmu_pmem_entry + (*PG_MIDDLE_DIR >> 2) * PAGE_SIZE + PG_TABLE_OFFSET;

    /*
    *   Page fault in Page
    *   Create Page
    */
    if (*PG_TABLE == UNMAPPED) {
        PAGE_t *new_PG;
        // pmem is full
        if (DEQUEUE_pmemlist(&new_PG) == 0)
            if (SWAP_OUT(&new_PG) == 0)
                return -1;

        *PG_TABLE = (((void *)new_PG - ku_mmu_pmem_entry) / PAGE_SIZE << 2) | PRESENT_MASK;

        ENQUEUE_pagelist(PG_TABLE);
        return 0;
    }

    /*
    *   Current page is swapped out
    */
    if ((*PG_TABLE & PRESENT_MASK) == 0) {
        ku_pte *ENTRY_OF_PAGE_TO_BE_SWAPPED_OUT;

        // FIFO
        if (DEQUEUE_pagelist(&ENTRY_OF_PAGE_TO_BE_SWAPPED_OUT) == 0)
            return -1;

        int PAGE_OFFSET = *ENTRY_OF_PAGE_TO_BE_SWAPPED_OUT >> 2;
        PAGE_t *PAGE_TO_BE_SWAPPED_OUT = ku_mmu_pmem_entry + ((*ENTRY_OF_PAGE_TO_BE_SWAPPED_OUT >> 2) * PAGE_SIZE);

        int SWAPPED_OFFSET = (*PG_TABLE >> 1) * PAGE_SIZE;
        PAGE_t *ENTRY_OF_SWAPPED_PAGE = ku_mmu_swap_entry + SWAPPED_OFFSET;

        SWAP_MEM(*PAGE_TO_BE_SWAPPED_OUT, *ENTRY_OF_PAGE_TO_BE_SWAPPED_OUT);

        *ENTRY_OF_PAGE_TO_BE_SWAPPED_OUT = *PG_TABLE;

        *PG_TABLE = (PAGE_OFFSET << 2) | PRESENT_MASK;

        ENQUEUE_pagelist(PG_TABLE);

        return 0;
    }
    return -1;
}

/*****************************************************************
*                         Context Switch                         *
******************************************************************/

int ku_run_proc(char pid, void **ku_cr3) {
    PCB *pcb_to_switch = SEARCH_pcblist(pid);

    /*
    *   Process with given pid is NOT available
    *   Creates pcb
    */
    if (pcb_to_switch == NULL) {
        PAGE_t *SWAPPED_PMEM;

        /*
        *   Size of PCB is *pdbr(8byte) + pid(1byte) -> TOTAL 9byte??
        *   Structure's memory is allocated by MULTIPLE OF BIGGEST MEMBER which is 8byte for this prj
        *   So Total size of struct PCB is 16Bytes.
        *   4byte has been allocated so 12 bytes are more needed.
        */
        PCB *new_PCB;
        if (DEQUEUE_pmemlist((PAGE_t **)&new_PCB) == 0)
            return -1;
        PAGE_t *buffer;
        for (int i = 0; i < 2; i++)
            if (DEQUEUE_pmemlist(&buffer) == 0)
                return -1;

        PAGE_t *PMEM;
        // pmem is full
        if (DEQUEUE_pmemlist(&PMEM) == 0) {
            if (SWAP_OUT(&SWAPPED_PMEM) == 0)
                return -1;

            new_PCB->pdbr = SWAPPED_PMEM;
            new_PCB->pid = pid;
            ADD_pcblist(new_PCB);
            *ku_cr3 = new_PCB->pdbr;

            return 0;
        }

        /*
        * Allocate from physical memory freelist
        */
        new_PCB->pdbr = PMEM;
        new_PCB->pid = pid;
        ADD_pcblist(new_PCB);
        *ku_cr3 = new_PCB->pdbr;

        return 0;
    }

    /*
    *  Process with given pid is available
    */
    *ku_cr3 = pcb_to_switch->pdbr;
    return 0;
}
