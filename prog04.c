#include "prog04.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VIRTUAL_BITS 20
#define MIN_PAGE_BITS 10
#define MAX_REFERENCE_COUNT 10
#define INITIAL_REFERENCE_COUNT 3

static void decrease_reference_counts(Simulator *sim)
{
    size_t i;

    for (i = 0; i < sim->frame_count; ++i) {
        Page *page = sim->frames[i].page;

        if (page != NULL && page->reference_count > 0) {
            --page->reference_count;
        }
    }
}

static Page *find_page(Simulator *sim, uint32_t page_number)
{
    size_t i;

    for (i = 0; i < sim->frame_count; ++i) {
        Page *page = sim->frames[i].page;

        if (page != NULL && page->number == page_number) {
            return page;
        }
    }

    return NULL;
}

static size_t find_frame_for_page(Simulator *sim)
{
    size_t i;
    size_t oldest_frame = 0;
    uint64_t oldest_order = UINT64_MAX;

    /*
     * Empty frames are always used before replacement.
     */
    for (i = 0; i < sim->frame_count; ++i) {
        if (sim->frames[i].page == NULL) {
            return i;
        }
    }

    /*
     * FIFO among pages whose reference count is zero.
     *
     * If no page has reference count zero, decrease every page's
     * reference count and try again.
     */
    for (;;) {
        bool found = false;

        oldest_order = UINT64_MAX;

        for (i = 0; i < sim->frame_count; ++i) {
            Page *page = sim->frames[i].page;

            if (page != NULL &&
                page->reference_count == 0 &&
                page->load_order < oldest_order) {

                oldest_order = page->load_order;
                oldest_frame = i;
                found = true;
            }
        }

        if (found) {
            return oldest_frame;
        }

        decrease_reference_counts(sim);
    }
}

static int parse_address(const char *text, uint64_t *address)
{
    char *end = NULL;
    unsigned long long value;

    if (text == NULL || *text == '\0') {
        return -1;
    }

    errno = 0;
    value = strtoull(text, &end, 10);

    if (errno != 0 || end == text) {
        return -1;
    }

    while (*end != '\0') {
        if (!isspace((unsigned char)*end)) {
            return -1;
        }

        ++end;
    }

    *address = (uint64_t)value;

    return 0;
}

int simulator_init(Simulator *sim,
                   unsigned virtual_bits,
                   unsigned page_bits,
                   size_t frame_count)
{
    if (sim == NULL ||
        frame_count == 0 ||
        virtual_bits > MAX_VIRTUAL_BITS ||
        page_bits < MIN_PAGE_BITS ||
        page_bits > virtual_bits) {

        return -1;
    }

    memset(sim, 0, sizeof(*sim));

    sim->virtual_bits = virtual_bits;
    sim->page_bits = page_bits;

    sim->virtual_size = UINT64_C(1) << virtual_bits;
    sim->page_size = UINT64_C(1) << page_bits;

    sim->frame_count = frame_count;

    sim->frames = calloc(frame_count, sizeof(*sim->frames));

    if (sim->frames == NULL) {
        return -1;
    }

    return 0;
}

void simulator_destroy(Simulator *sim)
{
    size_t i;

    if (sim == NULL) {
        return;
    }

    if (sim->frames != NULL) {
        for (i = 0; i < sim->frame_count; ++i) {
            free(sim->frames[i].page);
            sim->frames[i].page = NULL;
        }

        free(sim->frames);
        sim->frames = NULL;
    }
}

int simulator_run(Simulator *sim, const char *filename, bool debug)
{
    FILE *fp;
    char address_text[128];
    char operation;

    unsigned accesses_since_decrease = 0;

    if (sim == NULL || filename == NULL) {
        return -1;
    }

    fp = fopen(filename, "r");

    if (fp == NULL) {
        fprintf(stderr,
                "Error: cannot open input file '%s': %s\n",
                filename,
                strerror(errno));

        return -1;
    }

    if (debug) {
        printf("// Simulation Starts\n");
    }

    for (;;) {
        int result;
        uint64_t address;
        uint64_t page_number64;
        Page *page;
        size_t frame_index;

        result = fscanf(fp,
                        " %127s %c",
                        address_text,
                        &operation);

        if (result == EOF) {
            break;
        }

        if (result != 2) {
            fprintf(stderr,
                    "Error: invalid input line near access %" PRIu64 "\n",
                    sim->memory_accesses + 1);

            fclose(fp);
            return -1;
        }

        if (operation != 'r' && operation != 'w') {
            fprintf(stderr,
                    "Error: operation must be 'r' or 'w' at access %" PRIu64 "\n",
                    sim->memory_accesses + 1);

            fclose(fp);
            return -1;
        }

        if (parse_address(address_text, &address) != 0 ||
            address >= sim->virtual_size) {

            fprintf(stderr,
                    "Error: invalid address '%s' at access %" PRIu64
                    " (valid range: 0-%" PRIu64 ")\n",
                    address_text,
                    sim->memory_accesses + 1,
                    sim->virtual_size - 1);

            fclose(fp);
            return -1;
        }

        page_number64 = address / sim->page_size;

        if (page_number64 > UINT32_MAX) {
            fprintf(stderr,
                    "Error: page number is too large.\n");

            fclose(fp);
            return -1;
        }

        page = find_page(sim, (uint32_t)page_number64);

        /*
         * Page already exists in physical memory.
         */
        if (page != NULL) {

            /*
             * Initial loading does not increment the reference count.
             * Subsequent accesses do.
             */
            if (page->reference_count < MAX_REFERENCE_COUNT) {
                ++page->reference_count;
            }

            if (operation == 'w') {
                page->dirty = true;
            }
        }

        /*
         * Page fault.
         */
        else {
            Page *new_page;
            Page *old_page;

            frame_index = find_frame_for_page(sim);

            old_page = sim->frames[frame_index].page;

            /*
             * A non-empty frame means an actual replacement.
             */
            if (old_page != NULL) {

                ++sim->page_faults;

                if (debug) {
                    printf("Page %" PRIu32
                           " replaced by Page %" PRIu64 "\n",
                           old_page->number,
                           page_number64);

                    printf("Page %" PRIu32
                           " was %sdirty\n",
                           old_page->number,
                           old_page->dirty ? "" : "not ");
                }

                /*
                 * Only dirty pages actually written out during
                 * replacement count as disk writes.
                 */
                if (old_page->dirty) {
                    ++sim->pages_written_to_disk;
                }

                free(old_page);
            }

            /*
             * Empty frame.
             */
            else if (debug) {
                printf("Page NULL replaced by Page %" PRIu64 "\n",
                       page_number64);
            }

            new_page = malloc(sizeof(*new_page));

            if (new_page == NULL) {
                fprintf(stderr,
                        "Error: out of memory.\n");

                fclose(fp);
                return -1;
            }

            /*
             * Every newly loaded page starts with reference count 3.
             */
            new_page->number = (uint32_t)page_number64;
            new_page->reference_count = INITIAL_REFERENCE_COUNT;
            new_page->dirty = (operation == 'w');

            /*
             * load_order is used to implement FIFO.
             */
            new_page->load_order = sim->next_load_order++;

            sim->frames[frame_index].page = new_page;
        }

        ++sim->memory_accesses;
        ++accesses_since_decrease;

        /*
         * After every 4 memory accesses, decrease the reference
         * count of every page by one.
         */
        if (accesses_since_decrease == 4) {
            decrease_reference_counts(sim);
            accesses_since_decrease = 0;
        }
    }

    fclose(fp);

    if (debug) {
        printf("// Simulation Ends\n\n");
    }

    printf("// Display Statistics\n");

    printf("Number of memory accesses = %" PRIu64 "\n",
           sim->memory_accesses);

    printf("Number of memory accesses that resulted in page faults = %" PRIu64 "\n",
           sim->page_faults);

    printf("Number of pagees written to disk = %" PRIu64 "\n",
           sim->pages_written_to_disk);

    return 0;
}

static int parse_unsigned(const char *text, unsigned *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || *text == '\0') {
        return -1;
    }

    errno = 0;

    parsed = strtoul(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed > UINT_MAX) {

        return -1;
    }

    *value = (unsigned)parsed;

    return 0;
}

static int parse_size(const char *text, size_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || *text == '\0') {
        return -1;
    }

    errno = 0;

    parsed = strtoull(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        parsed == 0 ||
        parsed > SIZE_MAX) {

        return -1;
    }

    *value = (size_t)parsed;

    return 0;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s <virtual_bits> <page_bits> <frames> "
            "<tracefile> <d|n>\n"
            "Example: %s 20 12 5 pc4_tc1.txt d\n",
            program,
            program);
}

int main(int argc, char *argv[])
{
    unsigned virtual_bits;
    unsigned page_bits;
    size_t frame_count;

    bool debug;

    Simulator sim;

    int rc;

    if (argc != 6) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (parse_unsigned(argv[1], &virtual_bits) != 0 ||
        parse_unsigned(argv[2], &page_bits) != 0 ||
        parse_size(argv[3], &frame_count) != 0) {

        fprintf(stderr,
                "Error: invalid numeric argument.\n");

        print_usage(argv[0]);

        return EXIT_FAILURE;
    }

    /*
     * Assignment requirement:
     * virtual memory is at most 2^20.
     */
    if (virtual_bits > MAX_VIRTUAL_BITS) {
        fprintf(stderr,
                "Error: virtual memory is limited to 2^20 bytes.\n");

        return EXIT_FAILURE;
    }

    /*
     * Assignment requirement:
     * page size is at least 2^10.
     */
    if (page_bits < MIN_PAGE_BITS ||
        page_bits > virtual_bits) {

        fprintf(stderr,
                "Error: page size exponent must be at least 10 "
                "and not exceed the virtual memory exponent.\n");

        return EXIT_FAILURE;
    }

    /*
     * Debug flag must be d or n.
     */
    if (strlen(argv[5]) != 1 ||
        (argv[5][0] != 'd' &&
         argv[5][0] != 'n' &&
         argv[5][0] != 'D' &&
         argv[5][0] != 'N')) {

        fprintf(stderr,
                "Error: last argument must be 'd' or 'n'.\n");

        return EXIT_FAILURE;
    }

    debug = (argv[5][0] == 'd' ||
             argv[5][0] == 'D');

    if (simulator_init(&sim,
                       virtual_bits,
                       page_bits,
                       frame_count) != 0) {

        fprintf(stderr,
                "Error: unable to initialize simulator.\n");

        return EXIT_FAILURE;
    }

    rc = simulator_run(&sim,
                       argv[4],
                       debug);

    simulator_destroy(&sim);

    return (rc == 0)
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}