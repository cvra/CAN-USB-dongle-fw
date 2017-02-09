#!/bin/sh

# abort on errors
set -e
set -u

# All paths are relative to this script
cd $(dirname $0)

echo "// AUTOMATICALLY GENERATED" > src/version.c.new
echo "#include <board.h>" >> src/version.c.new
echo "const char * software_version_str = \"$(git rev-parse HEAD)-$(git rev-parse --abbrev-ref HEAD)\";" >> src/version.c.new
echo "const char * hardware_version_str = BOARD_NAME;" >> src/version.c.new

# If version.c does not exist, overwrite it
if [ ! -e src/version.c ]
then
    echo "version.c does not exist, creating it..."
    mv src/version.c.new src/version.c
else
    # If there is a version.c, check if the file should change before
    # overriding it.
    # If we don't do this, make will detect a new file and always relink the
    # project.
    if cmp --quiet src/version.c src/version.c.new
    then
        echo "No change detected in version.c, skipping..."
        rm src/version.c.new
    else
        echo "version.c changed, overwriting it"
        mv src/version.c.new src/version.c
    fi
fi
