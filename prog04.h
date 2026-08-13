#ifndef PROG04_H
#define PROG04_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Page {
    uint32_t number;

    int reference_count;

    bool dirty;

    uint64_t load_order;
} Page;

typedef struct Frame {
    Page *page;
} Frame;

typedef struct Simulator {
    unsigned virtual_bits;
    unsigned page_bits;

    uint64_t virtual_size;
    uint64_t page_size;

    size_t frame_count;

    Frame *frames;

    uint64_t memory_accesses;
    uint64_t page_faults;
    uint64_t pages_written_to_disk;

    uint64_t next_load_order;
} Simulator;

int simulator_init(Simulator *sim,
                   unsigned virtual_bits,
                   unsigned page_bits,
                   size_t frame_count);

void simulator_destroy(Simulator *sim);

int simulator_run(Simulator *sim,
                  const char *filename,
                  bool debug);

#endif