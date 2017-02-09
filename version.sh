#!/bin/sh

echo "// AUTOMATICALLY GENERATED" > src/version.c
echo "#include <board.h>" >> src/version.c
echo "const char * software_version_str = \"$(git rev-parse HEAD)-$(git rev-parse --abbrev-ref HEAD)\";" >> src/version.c
echo "const char * hardware_version_str = BOARD_NAME;" >> src/version.c
