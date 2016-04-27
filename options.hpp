static int additional_options[] = {
#define A_COALESCE ((int) 'c')
	A_COALESCE,
#define A_REVERSE ((int) 'r')
	A_REVERSE,
#define A_REORDER ((int) 'o')
	A_REORDER,
#define A_LINE_DROP ((int) 'l')
	A_LINE_DROP,
#define A_DEBUG_POLYGON ((int) 'd')
	A_DEBUG_POLYGON,
#define A_POLYGON_DROP ((int) 'p')
	A_POLYGON_DROP,
#define A_PREFER_RADIX_SORT ((int) 'R')
	A_PREFER_RADIX_SORT,
};

static int prevent_options[] = {
#define P_SIMPLIFY ((int) 's')
	P_SIMPLIFY,
#define P_SIMPLIFY_LOW ((int) 'S')
	P_SIMPLIFY_LOW,
#define P_FEATURE_LIMIT ((int) 'f')
	P_FEATURE_LIMIT,
#define P_KILOBYTE_LIMIT ((int) 'k')
	P_KILOBYTE_LIMIT,
#define P_DYNAMIC_DROP ((int) 'd')
	P_DYNAMIC_DROP,
#define P_INPUT_ORDER ((int) 'i')
	P_INPUT_ORDER,
#define P_POLYGON_SPLIT ((int) 'p')
	P_POLYGON_SPLIT,
#define P_CLIPPING ((int) 'c')
	P_CLIPPING,
#define P_DUPLICATION ((int) 'D')
	P_DUPLICATION,
};
