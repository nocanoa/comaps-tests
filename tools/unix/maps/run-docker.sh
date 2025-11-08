#!/usr/bin/env bash

# Master file documenting how to clone, build, and run the maps generator via Docker

# Prerequisutes:
# sudo apt install docker git

# To bootstrap the repo:
#cd /mnt/4tbexternal
#if [ ! -f /mnt/4tbexternal/comaps ]; then
#  git clone --recurse-submodules --shallow-submodules https://codeberg.org/comaps/comaps.git
#  cd /mnt/4tbexternal/comaps
#else
#  cd /mnt/4tbexternal/comaps
#  git pull origin
#fi

# And data:
# cd /mnt/4tbexternal/comaps/data/
# wget World.mwm //pastk - not needed
# wget WorldCoasts.mwm

#TODO: isolines, postcodes, subways, wiki

# In tools/unix/maps, copy settings.sh.dist to settings.sh and modify if needed
# In tools/python/maps_generator/var/etc, copy map_generator.ini.prod to map_generator.ini and modify if needed

#cd /mnt/4tbexternal/comaps/tools/unix/maps
# Build with: docker build . -t maps_generator
# (Good to rebuild each time just in case)
# To push for ci/cd, tag for codeberg:
#   docker login codeberg.org
#   docker tag maps_generator codeberg.org/comaps/maps_generator:latest
#   docker push codeberg.org/comaps/maps_generator:latest
# You can also tag and push the image Id for posterity: codeberg.org/comaps/maps_generator:1234abcd
# If you get a Dockerfile not found error especially on an XFS partition, try copying Dockerfile to an ext4 partition first.
# Or use docker via apt instead of snap.
#
# Edit as appropriate and run with:
# docker run \
#   -e S3_KEY_ID=changeme \
#   -e S3_SECRET_KEY=changeme \
#   -e S3_ENDPOINT=https://changeme.r2.cloudflarestorage.com/ \
#   -e S3_BUCKET=comaps-map-files \
#   -e SFTP_USER=changeme \
#   -e SFTP_PASSWORD=changeme \
#   -e SFTP_HOST=changeme.dreamhost.com \
#   -e SFTP_PATH=cdn-us-1.comaps.app \
#   --ulimit nofile=262144:262144 \
#   -v /mnt/4tbexternal/comaps:/mnt/4tbexternal/comaps \
#   -v /mnt/4tbexternal/osm-planet:/home/planet \
#   -v /mnt/4tbexternal/osm-maps:/mnt/4tbexternal/osm-maps \
#   -it maps_generator \
#   /mnt/4tbexternal/comaps/tools/unix/docker_maps_generator.sh
 
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
