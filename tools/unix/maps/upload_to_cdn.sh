#!/usr/bin/env bash

# Upload new maps version to all CDN nodes (in parallel).

# Use following commands for deleting older maps:
#
# ru1 - keep max 3 maps versions
# First list all maps versions on the server
#   sudo rclone lsd ru1:comaps-maps/maps
# Delete the old version
#   sudo rclone purge -v ru1:comaps-maps/maps/250713
#
# fi1 - max 3 versions
#   sudo rclone lsd fi1:/var/www/html/maps
#   sudo rclone purge -v fi1:/var/www/html/maps/250713
#
# de1 - max 6 versions
#    sudo rclone lsd de1:/var/www/html/comaps-cdn/maps
#    sudo rclone purge -v de1:/var/www/html/comaps-cdn/maps/250713
#
# us2 - all versions, don't delete
#    sudo rclone lsd us2:comaps-map-files/maps

set -e -u

if [ $# -eq 0 ]; then
  echo "Usage: upload_to_cdn.sh MAPS_PATH"
  echo "e.g. sudo upload_to_cdn.sh osm-maps/2025_09_06__09_48_08/250906"
  echo "uploads are run in parallel to us2,ru1,fi1,de1 servers,"
  echo "subsequent runs will update only missing/differing files,"
  echo "so its fine to run second time to ensure there were no incomplete transfers"
  echo "or to run on an unfinished generation first and then again after its fully finished."
  echo "(sudo is needed to access rclone.conf with servers credentials)"
  exit 1
fi

MAPS=$(basename $1)
DIR=$(dirname $1)/$MAPS

echo "Uploading maps folder $DIR to $MAPS"

echo "Uploading to us2"
# An explicit mwm/txt filter is used to skip temp files when run for an unfinished generation
rclone copy -v --include "*.{mwm,txt}" $DIR us2:comaps-map-files/maps/$MAPS &

echo "Uploading to ru1"
rclone copy -v --include "*.{mwm,txt}" $DIR ru1:comaps-maps/maps/$MAPS &

echo "Uploading to fi1"
rclone copy -v --include "*.{mwm,txt}" $DIR fi1:/var/www/html/maps/$MAPS &

echo "Uploading to de1"
rclone copy -v --include "*.{mwm,txt}" $DIR de1:/var/www/html/comaps-cdn/maps/$MAPS &

# us1 is not used for maps atm
# rclone lsd us1:/home/dh_zzxxrk/cdn-us-1.comaps.app/maps

wait

echo "Upload complete"
