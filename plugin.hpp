mvt_layer filter_layer(const char *filter, mvt_layer &layer, unsigned z, unsigned x, unsigned y);
void setup_filter(const char *filter, int *write_to, int *read_from, pid_t *pid, unsigned z, unsigned x, unsigned y);
