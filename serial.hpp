size_t fwrite_check(const void *ptr, size_t size, size_t nitems, FILE *stream, const char *fname);

void serialize_int(FILE *out, int n, long long *fpos, const char *fname);
void serialize_long_long(FILE *out, long long n, long long *fpos, const char *fname);
void serialize_byte(FILE *out, signed char n, long long *fpos, const char *fname);
void serialize_uint(FILE *out, unsigned n, long long *fpos, const char *fname);
void serialize_string(FILE *out, const char *s, long long *fpos, const char *fname);

void deserialize_int(char **f, int *n);
void deserialize_long_long(char **f, long long *n);
void deserialize_uint(char **f, unsigned *n);
void deserialize_byte(char **f, signed char *n);

int deserialize_int_io(FILE *f, int *n, long long *geompos);
int deserialize_long_long_io(FILE *f, long long *n, long long *geompos);
int deserialize_uint_io(FILE *f, unsigned *n, long long *geompos);
int deserialize_byte_io(FILE *f, signed char *n, long long *geompos);
