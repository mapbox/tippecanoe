struct parse_json_args {
	json_pull *jp;
	const char *reading;
	volatile long long *layer_seq;
	volatile long long *progress_seq;
	long long *metapos;
	long long *geompos;
	long long *indexpos;
	std::set<std::string> *exclude;
	std::set<std::string> *include;
	int exclude_all;
	FILE *metafile;
	FILE *geomfile;
	FILE *indexfile;
	struct memfile *poolfile;
	struct memfile *treefile;
	char *fname;
	int basezoom;
	int layer;
	double droprate;
	long long *file_bbox;
	int segment;
	int *initialized;
	unsigned *initial_x;
	unsigned *initial_y;
	struct reader *readers;
	int maxzoom;
	std::map<std::string, layermap_entry> *layermap;
	std::string *layername;
	bool uses_gamma;
};

struct json_pull *json_begin_map(char *map, long long len);
void json_end_map(struct json_pull *jp);

void parse_json(json_pull *jp, const char *reading, volatile long long *layer_seq, volatile long long *progress_seq, long long *metapos, long long *geompos, long long *indexpos, std::set<std::string> *exclude, std::set<std::string> *include, int exclude_all, FILE *metafile, FILE *geomfile, FILE *indexfile, struct memfile *poolfile, struct memfile *treefile, char *fname, int basezoom, int layer, double droprate, long long *file_bbox, int segment, int *initialized, unsigned *initial_x, unsigned *initial_y, struct reader *readers, int maxzoom, std::map<std::string, layermap_entry> *layermap, std::string layername, bool uses_gamma);
void *run_parse_json(void *v);
