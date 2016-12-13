mvt_layer filter_layer(const char *filter, mvt_layer &layer, unsigned z, unsigned x, unsigned y, std::vector<std::map<std::string, layermap_entry>> *layermaps, size_t tiling_seg);
void setup_filter(const char *filter, int *write_to, int *read_from, pid_t *pid, unsigned z, unsigned x, unsigned y);
serial_feature parse_feature(json_pull *jp, unsigned z, unsigned x, unsigned y, std::vector<std::map<std::string, layermap_entry>> *layermaps, size_t tiling_seg);
