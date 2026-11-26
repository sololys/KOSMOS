#include "kosmos_channel_sluice.h"

int kosmos_channel_valid(uint16_t id) {
    return id < KOSMOS_CHANNEL_COUNT;
}

int kosmos_channel_graph_valid(const kosmos_channel_edge_t *edges, size_t count,
                               uint16_t max_degree) {
    uint16_t degree[KOSMOS_CHANNEL_COUNT] = {0};
    uint8_t seen[KOSMOS_CHANNEL_COUNT][KOSMOS_CHANNEL_COUNT / 8] = {{0}};

    if (max_degree >= KOSMOS_CHANNEL_COUNT || (count != 0u && edges == NULL)) {
        return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        uint16_t a = edges[i].a;
        uint16_t b = edges[i].b;
        if (!kosmos_channel_valid(a) || !kosmos_channel_valid(b) || a == b) {
            return 0;
        }
        if (a > b) {
            const uint16_t swap = a;
            a = b;
            b = swap;
        }
        const uint8_t bit = (uint8_t)(1u << (b % 8u));
        if ((seen[a][b / 8u] & bit) != 0u ||
            degree[a] == max_degree || degree[b] == max_degree) {
            return 0;
        }
        seen[a][b / 8u] |= bit;
        ++degree[a];
        ++degree[b];
    }
    return 1;
}

int kosmos_channel_is_trapped(const kosmos_channel_sluice_t *sluice, uint16_t id) {
    return kosmos_channel_valid(id) && sluice->trapped[id] != 0u;
}

void kosmos_channel_trap(kosmos_channel_sluice_t *sluice, uint16_t id) {
    if (kosmos_channel_valid(id)) {
        sluice->trapped[id] = 1u;
    }
}
