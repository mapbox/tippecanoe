#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#define A_COALESCE ((int32_t) 'c')
#define A_REVERSE ((int32_t) 'r')
#define A_REORDER ((int32_t) 'o')
#define A_LINE_DROP ((int32_t) 'l')
#define A_DEBUG_POLYGON ((int32_t) '@')
#define A_POLYGON_DROP ((int32_t) 'p')
#define A_DETECT_SHARED_BORDERS ((int32_t) 'b')
#define A_PREFER_RADIX_SORT ((int32_t) 'R')
#define A_CALCULATE_FEATURE_DENSITY ((int32_t) 'g')
#define A_INCREASE_GAMMA_AS_NEEDED ((int32_t) 'G')
#define A_MERGE_POLYGONS_AS_NEEDED ((int32_t) 'm')
#define A_DROP_DENSEST_AS_NEEDED ((int32_t) 's')
#define A_DROP_FRACTION_AS_NEEDED ((int32_t) 'd')
#define A_DROP_SMALLEST_AS_NEEDED ((int32_t) 'n')
#define A_COALESCE_DENSEST_AS_NEEDED ((int32_t) 'S')
#define A_COALESCE_SMALLEST_AS_NEEDED ((int32_t) 'N')
#define A_COALESCE_FRACTION_AS_NEEDED ((int32_t) 'D')
#define A_GRID_LOW_ZOOMS ((int32_t) 'L')
#define A_DETECT_WRAPAROUND ((int32_t) 'w')
#define A_EXTEND_ZOOMS ((int32_t) 'e')
#define A_CLUSTER_DENSEST_AS_NEEDED ((int32_t) 'C')
#define A_GENERATE_IDS ((int32_t) 'i')
#define A_CONVERT_NUMERIC_IDS ((int32_t) 'I')
#define A_HILBERT ((int32_t) 'h')

#define P_SIMPLIFY ((int32_t) 's')
#define P_SIMPLIFY_LOW ((int32_t) 'S')
#define P_FEATURE_LIMIT ((int32_t) 'f')
#define P_KILOBYTE_LIMIT ((int32_t) 'k')
#define P_DYNAMIC_DROP ((int32_t) 'd')
#define P_INPUT_ORDER ((int32_t) 'i')
#define P_POLYGON_SPLIT ((int32_t) 'p')
#define P_CLIPPING ((int32_t) 'c')
#define P_DUPLICATION ((int32_t) 'D')
#define P_TINY_POLYGON_REDUCTION ((int32_t) 't')
#define P_TILE_COMPRESSION ((int32_t) 'C')
#define P_TILE_STATS ((int32_t) 'g')
#define P_USE_SOURCE_POLYGON_WINDING ((int32_t) 'w')
#define P_REVERSE_SOURCE_POLYGON_WINDING ((int32_t) 'W')
#define P_EMPTY_CSV_COLUMNS ((int32_t) 'e')

extern int32_t prevent[256];
extern int32_t additional[256];

#endif
