void layer_to_geojson(FILE *fp, mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, size_t index, size_t sequence, long extent, bool complain);
void fprintq(FILE *f, const char *s);
