# KU_MMU

Implementation of Memory Management Unit at application level  
Assignment in Konkuk Univ. Operating System class by Prof. Hyun-Wook Jin

---
   
## Introduction

Due to the requirements of assignment, every variables and functions are written in single file: [ku_mmu.h](./ku_mmu.h). Which is a huge violation of modern software architecture :(

</br>

Application-level implementation of Memory Management Unit.

Converts given virtual address with PID to physical address in [input.txt](./input.txt)

- **8-bit** addressing: **256B** of address space
- Page size: 4B
- PDE/PTE: 1B

---

## Data Structures

### PCB List

![pcb_list](./images/pcb_list.jpg)

<br/>

### Memory Free list

![pmem_list](./images/pmem_list.jpg)

![swap_list](./images/swap_list.jpg)

### Init status

![init](./images/bitPacking_in_initialization.PNG)

<br/>

### Mapped memory

![mapping](./images/mapping.jpg)

</br>   
   
### Swap out   
  
- Swap space has free memory (default)

![default_swap](./images/default_swap.PNG)

- Swap space is full

![swap_space_is_full](./images/swap_when_swapSpace_is_full.PNG)

---

## Run

`$ ku_cpu <input_file> <pmem_size> <swap_size>`
