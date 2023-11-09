# Hierarchical-Page-Tables
![image](https://github.com/YuvalShaffir/Hierarchical-Page-Tables/assets/34415892/50a6ec5f-e65e-4cb2-8248-1dd90ababd62)

## Introduction
This virtual memory interface allows processes to use more memory than physically available on the host through address translation and paging. It employs a hierarchical page table structure, providing efficient memory management.

## Features

Address Translation:
Virtual memory addresses are translated to physical addresses using hierarchical page tables.
Paging enables efficient use of memory space.

Page Fault Handling:
Detection of page faults when a virtual page or its parent tables are not in physical memory.
Swapping pages into physical memory and creating tables as needed.

Finding Frames:
Prioritized selection of frames for page insertion or eviction:
Empty table frame, removing the reference from its parent.
Unused frame based on the maximal frame index.
If all frames are used, choose a frame by evaluating the cyclic distance from the page to be swapped out.

## How to Use

Initialization:
Call VMInitialize() to clear frame 0.

Read and Write Operations:
Use PMread and PMwrite for reading from and writing to virtual memory addresses.

Page Fault Handling:
The provided example demonstrates how to handle page faults during write operations.

Finding Frames:
The framework automatically handles the selection of frames based on the described prioritization.
Ensure to follow the provided structure and guidelines for efficient implementation.
