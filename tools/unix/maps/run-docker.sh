#!/usr/bin/env bash

# Run the maps generator via Docker manually without CI
# See .forgejo/workflows/map-generator.yml for steps to run before the main mapgen process,
# e.g. clone the repos, get/update planet file, generate isolines etc.

# To build the docker container:
# cd /mnt/4tbexternal/comaps/tools/unix/maps
# docker build . -t maps_generator
#
# To push for ci/cd, tag for codeberg:
#   docker login codeberg.org
#   docker tag maps_generator codeberg.org/comaps/maps_generator:latest
#   docker push codeberg.org/comaps/maps_generator:latest
# You can also tag and push the image Id for posterity: codeberg.org/comaps/maps_generator:1234abcd
# If you get a Dockerfile not found error especially on an XFS partition, try copying Dockerfile to an ext4 partition first.
# Or use docker via apt instead of snap.

docker run \
  -e S3_KEY_ID=changeme \
  --ulimit nofile=262144:262144 \
  -v /mnt/4tbexternal/comaps:/mnt/4tbexternal/comaps \
  -v /mnt/4tbexternal/wikiparser:/mnt/4tbexternal/wikiparser \
  -v /mnt/4tbexternal/osm-planet:/home/planet \
  -v /mnt/4tbexternal/osm-maps:/mnt/4tbexternal/osm-maps \
  -v /mnt/4tbexternal/subways:/mnt/4tbexternal/subways \
  -v /mnt/4tbexternal/omim-build-relwithdebinfo:/mnt/4tbexternal/omim-build-relwithdebinfo \
  -it codeberg.org/comaps/maps_generator:latest \
  /mnt/4tbexternal/comaps/tools/unix/docker_maps_generator.sh
