# NOTE: edit the settings.sh file to customize/override the defaults.

# Absolutize & normalize paths.
REPO_PATH="${REPO_PATH:-$(cd "$(dirname "$0")/../../.."; pwd -P)}"

BASE_PATH="${BASE_PATH:-$REPO_PATH/../maps}"
# Temporary files
BUILD_PATH="${BUILD_PATH:-$BASE_PATH/build}"
# Other code repositories, e.g. subways, wikiparser..
CODE_PATH="${CODE_PATH:-$REPO_PATH/..}"
# Source map data and processed outputs e.g. wiki articles
DATA_PATH="${DATA_PATH:-$BASE_PATH/data}"
