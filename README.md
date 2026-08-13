# Virtual Memory Management Simulator

## Project Overview

This project implements a Virtual Memory Management Simulator in C.

The simulator models:

- Virtual memory
- Pages
- Physical memory frames
- Page faults
- Reference counts
- FIFO page replacement
- Dirty pages
- Disk writes

The implementation follows the assignment specification provided for `prog04`.

---

## Program Invocation

The program is executed as:

    prog04 <virtual_bits> <page_bits> <frames> <tracefile> <d|n>

Example:

    ./prog04 20 12 5 prog04.sampleinput.txt d

The parameters mean:

| Parameter | Meaning |
|---|---|
| `20` | Virtual memory is `2^20` bytes |
| `12` | Page size is `2^12` bytes |
| `5` | Physical memory contains 5 frames |
| `prog04.sampleinput.txt` | Memory access trace file |
| `d` | Debugging information enabled |

Use `n` instead of `d` to disable debugging output.

---

# Assignment Requirements

## Virtual Memory

The first command-line argument specifies the power of 2 for virtual memory.

For example:

    20

means:

    2^20 bytes

The assignment specifies that virtual memory is at most `2^20`.

---

## Page Size

The second command-line argument specifies the power of 2 for page size.

For example:

    12

means:

    2^12 bytes

The assignment specifies that page size is at least `2^10`.

---

## Physical Memory

The third command-line argument specifies the number of frames in physical memory.

For example:

    5

means physical memory contains five frames.

---

# Input File Format

Each line contains:

    ADDR r

or:

    ADDR w

where:

- `ADDR` is the memory address in decimal.
- `r` represents a read operation.
- `w` represents a write operation.

Example:

    356780 r
    354779 w
    600710 r

Blank lines are allowed at the end of the input file.

---

# Reference Count Algorithm

Every page that is loaded into memory starts with:

    reference count = 3

After every four memory accesses, the reference count of every page currently in memory is reduced by one.

The minimum reference count is:

    0

When an already-resident page is accessed again, its reference count increases by one.

The maximum reference count is:

    10

Important:

The first access that causes a page to be loaded does not increase its reference count.

The page starts at 3.

---

# Page Replacement Algorithm

The replacement algorithm is:

1. Use an empty frame if one is available.
2. Otherwise, only pages with reference count `0` can be replaced.
3. If multiple pages have reference count `0`, use FIFO.
4. FIFO means the page that was loaded earliest is selected.
5. If no page has reference count `0`, reduce every page's reference count by one.
6. Search for a replacement candidate again.
7. Repeat until a page with reference count `0` is available.

The implementation stores a `load_order` value for every page so FIFO can be performed efficiently.

---

# Dirty Pages

A page becomes dirty when it is accessed with a write operation.

For example:

    354779 w

marks the corresponding page as dirty.

When a dirty page is replaced, it is considered written to disk.

The disk-write counter is incremented.

When a clean page is replaced, no disk write is counted.

Dirty pages that remain in physical memory when the simulation finishes are ignored, as required by the assignment.

---

# Debugging Output

When the last argument is `d`, the simulator prints replacement information.

Example:

    Page NULL replaced by Page 87

If a page is replaced:

    Page 87 replaced by Page 24
    Page 87 was not dirty

For a dirty page:

    Page 86 replaced by Page 143
    Page 86 was dirty

When the last argument is `n`, these simulation/debugging messages are not printed.

---

# Statistics

At the end of the simulation, the program prints:

1. Number of memory accesses
2. Number of memory accesses that resulted in page faults
3. Number of pages written to disk

The assignment specifically says that page faults should only count when an existing page is replaced.

Therefore:

    Empty frame -> new page

does NOT count as a page fault in the final page-fault statistic.

However:

    Existing page -> new page

does count as a page fault.

---

# Sample Test

Build the program:

    make -f prog04.makefile

Run:

    ./prog04 20 12 5 prog04.sampleinput.txt d

Expected statistics:

    Number of memory accesses = 15
    Number of memory accesses that resulted in page faults = 6
    Number of pagees written to disk = 3

The supplied test case produces exactly these results.

---

# Project Files

    prog04.c
    prog04.h
    prog04.makefile
    prog04.sampleinput.txt
    expected_output.txt
    actual_output.txt
    README.md

---

# Compilation

Using the supplied makefile:

    make -f prog04.makefile

This produces:

    prog04

The compiler flags used are:

    -std=c11
    -Wall
    -Wextra
    -Wpedantic
    -O2

---

# Running Without Debugging

Use:

    ./prog04 20 12 5 prog04.sampleinput.txt n

Only the final statistics are displayed.

---

# Running With Debugging

Use:

    ./prog04 20 12 5 prog04.sampleinput.txt d

The simulator displays page loading, replacement, dirty-page status, and final statistics.

---

# Cleaning the Build

Run:

    make -f prog04.makefile clean

This removes the executable and object file.

---

# GitHub Repository Structure

Recommended repository:

    virtual-memory-management-simulator/
    |
    |-- prog04.c
    |-- prog04.h
    |-- prog04.makefile
    |-- prog04.sampleinput.txt
    |-- expected_output.txt
    |-- actual_output.txt
    `-- README.md

---

# Project Explanation

The simulator receives a virtual memory size, page size, physical memory frame count, and a memory access trace.

For each memory access, the virtual address is converted into a page number using:

    page_number = address / page_size

If the page is already present in physical memory, its reference count is updated and a write operation marks it dirty.

If the page is not present, a free frame is used if available. Otherwise, the replacement algorithm searches for a page with reference count zero and chooses the oldest eligible page.

When a dirty page is replaced, it is counted as a page written to disk.

At the end, the simulator displays the required statistics.

---

# Test Result

The supplied test case was compiled and executed successfully.

Result:

    Number of memory accesses = 15
    Number of memory accesses that resulted in page faults = 6
    Number of pagees written to disk = 3

These values match the supplied solution output.
