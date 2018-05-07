#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#define A_COALESCE ((int) 'c')
#define A_REVERSE ((int) 'r')
#define A_REORDER ((int) 'o')
#define A_LINE_DROP ((int) 'l')
#define A_DEBUG_POLYGON ((int) '@')
#define A_POLYGON_DROP ((int) 'p')
#define A_DETECT_SHARED_BORDERS ((int) 'b')
#define A_PREFER_RADIX_SORT ((int) 'R')
#define A_CALCULATE_FEATURE_DENSITY ((int) 'g')
#define A_INCREASE_GAMMA_AS_NEEDED ((int) 'G')
#define A_MERGE_POLYGONS_AS_NEEDED ((int) 'm')
#define A_DROP_DENSEST_AS_NEEDED ((int) 's')
#define A_DROP_FRACTION_AS_NEEDED ((int) 'd')
#define A_DROP_SMALLEST_AS_NEEDED ((int) 'n')
#define A_COALESCE_DENSEST_AS_NEEDED ((int) 'S')
#define A_COALESCE_SMALLEST_AS_NEEDED ((int) 'N')
#define A_COALESCE_FRACTION_AS_NEEDED ((int) 'D')
#define A_GRID_LOW_ZOOMS ((int) 'L')
#define A_DETECT_WRAPAROUND ((int) 'w')
#define A_EXTEND_ZOOMS ((int) 'e')
#define A_CLUSTER_DENSEST_AS_NEEDED ((int) 'C')
#define A_JOIN_FEATURES_ACROSS_TILES ((int) 'j')

#define P_SIMPLIFY ((int) 's')
#define P_SIMPLIFY_LOW ((int) 'S')
#define P_FEATURE_LIMIT ((int) 'f')
#define P_KILOBYTE_LIMIT ((int) 'k')
#define P_DYNAMIC_DROP ((int) 'd')
#define P_INPUT_ORDER ((int) 'i')
#define P_POLYGON_SPLIT ((int) 'p')
#define P_CLIPPING ((int) 'c')
#define P_DUPLICATION ((int) 'D')
#define P_TINY_POLYGON_REDUCTION ((int) 't')
#define P_TILE_COMPRESSION ((int) 'C')
#define P_TILE_STATS ((int) 'g')

extern int prevent[256];
extern int additional[256];

#endif
