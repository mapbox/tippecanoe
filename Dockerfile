# Start from ubuntu
FROM ubuntu:16.04

# Update repos and install dependencies
RUN apt-get update \
  && apt-get -y upgrade \
  && apt-get -y install build-essential libsqlite3-dev zlib1g-dev

# Create a directory and copy in all files
RUN mkdir -p /tmp/tippecanoe-src
WORKDIR /tmp/tippecanoe-src
COPY . /tmp/tippecanoe-src

# Build tippecanoe
RUN make \
  && make install

# Run the tests
CMD make test
