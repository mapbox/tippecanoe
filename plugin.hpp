void layer_to_geojson(FILE *fp, mvt_layer &layer, unsigned z, unsigned x, unsigned y);
mvt_layer filter_layer(const char *filter, mvt_layer &layer, unsigned z, unsigned x, unsigned y);
void fprintq(FILE *f, const char *s);
