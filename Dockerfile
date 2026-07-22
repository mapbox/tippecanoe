# Allow setting version as an argument
ARG UBUNTU_VERSION=20.04

# Start from ubuntu
FROM ubuntu:${UBUNTU_VERSION} as builder

# Update repos and install dependencies
RUN apt-get update \
  && apt-get -y install make gcc g++ libsqlite3-dev zlib1g-dev

# Copy in all files
WORKDIR /tmp/tippecanoe-src
COPY . /tmp/tippecanoe-src

# Build tippecanoe
RUN make \
  && make install

# Run the tests
CMD make test

# Build final image
FROM ubuntu:${UBUNTU_VERSION}

# Install runtime dependencies
RUN apt-get update \
  && apt-get install -y libsqlite3-0 zlib1g \
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/*

# Copy built files into final image
COPY --from=builder /usr/local/ /usr/local/
