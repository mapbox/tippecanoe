void layer_to_geojson(FILE *fp, mvt_layer const &layer, unsigned z, unsigned x, unsigned y, bool comma, bool name, bool zoom, bool dropped, unsigned long long index, long long sequence, long long extent, bool complain);
void fprintq(FILE *f, const char *s);
