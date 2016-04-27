struct index {
        long long start;
        long long end;
        unsigned long long index;
        short segment;
        unsigned long long seq : (64 - 16);  // pack with segment to stay in 32 bytes
};

void checkdisk(struct reader *r, int nreader);
