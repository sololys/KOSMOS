#ifndef KOSMOS_CHANNEL_SLUICE_H
#define KOSMOS_CHANNEL_SLUICE_H

#include <stddef.h>
#include <stdint.h>

enum { KOSMOS_CHANNEL_COUNT = 1000 };

typedef struct {
    uint16_t a;
    uint16_t b;
} kosmos_channel_edge_t;

typedef struct {
    uint8_t trapped[KOSMOS_CHANNEL_COUNT];
} kosmos_channel_sluice_t;

int kosmos_channel_valid(uint16_t id);
int kosmos_channel_graph_valid(const kosmos_channel_edge_t *edges, size_t count,
                               uint16_t max_degree);
int kosmos_channel_is_trapped(const kosmos_channel_sluice_t *sluice, uint16_t id);
void kosmos_channel_trap(kosmos_channel_sluice_t *sluice, uint16_t id);

#endif
