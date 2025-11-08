#!/usr/bin/env bash

set -e

echo "<$(date +%T)> Setting git as safe dir..."

# TODO: is it needed still? why?
git config --global --add safe.directory /mnt/4tbexternal/comaps

echo "<$(date +%T)> Starting..."

# Prepare paths
#
# Already created by Dockerfile or CI/CD:
#   /mnt/4tbexternal
#   /mnt/4tbexternal/comaps
#   /mnt/4tbexternal/omim-build-release
#   /mnt/4tbexternal/omim-build-relwithdebinfo
#   /mnt/4tbexternal/osm-maps
#   /home/planet
#   /home/planet/isolines
#   /home/planet/wikipedia
#   /home/planet/tiger
#
mkdir -p /root/.config/CoMaps # Odd mkdir permission errors in generator_tool in Docker without these
chmod -R 777 /root/.config
mkdir -p /home/planet/postcodes/gb-postcode-data/
mkdir -p /home/planet/postcodes/us-postcodes/
mkdir -p /home/planet/SRTM-patched-europe/
mkdir -p /home/planet/subway

echo "<$(date +%T)> Running ./configure.sh ..."
cd /mnt/4tbexternal/comaps
./configure.sh --skip-map-download --skip-generate-symbols

# TODO: output to the container, not to the mounted volume
echo "<$(date +%T)> Compiling tools..."
cd /mnt/4tbexternal/comaps
./tools/unix/build_omim.sh -R generator_tool
./tools/unix/build_omim.sh -R world_roads_builder_tool
./tools/unix/build_omim.sh -R mwm_diff_tool
cd tools/python/maps_generator
python3 -m venv /tmp/venv
/tmp/venv/bin/pip3 install -r requirements_dev.txt

echo "<$(date +%T)> Copying map generator INI..."
cp var/etc/map_generator.ini.prod var/etc/map_generator.ini

echo "<$(date +%T)> Generating maps..."
cd /mnt/4tbexternal/comaps/tools/python
/tmp/venv/bin/python -m maps_generator --skip="MwmDiffs"

# To continue from previous successfully accomplished stage:
#/tmp/venv/bin/python -m maps_generator --skip="MwmDiffs" --continue

# To generate only certain MWMs:
#/tmp/venv/bin/python -m maps_generator --countries="Macedonia, US_Oregon_*" --skip="MwmDiffs"

echo "<$(date +%T)> DONE"
