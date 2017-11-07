FROM centos:7

RUN yum install -y make sqlite-devel zlib-devel bash git gcc-c++

# Create a directory and copy in all files
RUN mkdir -p /tmp/tippecanoe-src
WORKDIR /tmp/tippecanoe-src
COPY . /tmp/tippecanoe-src

# Build tippecanoe
RUN make \
  && make install

# Run the tests
CMD make test
