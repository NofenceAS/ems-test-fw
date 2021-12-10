#!/bin/bash
#
# Script copied from old staging01, with some small tweaks
#
# Installs the firmware into the destination dir from the src dir.
#
# *The script assumes that the dest dir exist"
#
#
SRC_DIR=$1
DEST_DIR=$2

mkdir -p "$DEST_DIR"

# Be sure to compare new firmware versions. If the new release has built the same FW as
# the old, the difference must be NONE. An exception is zip files generated with time
# stamps

changed_files=`diff -r --brief -x '*.zip' -x '.DS_Store' --from-file="$SRC_DIR/"  "$DEST_DIR/" | grep '^Files'`
changed_files_count=`echo "$changed_files" | wc -l`

# We have to check if the changed_files are empty if not we are not able to know the difference
# between no diff and a single file diff
if [ -n "$changed_files" ] && [ "$changed_files_count" -gt 0 ]
then
     echo "**  Error in firmare - unexpected diff"
     echo "$changed_files"
     exit 1
fi

# Copy over all new files from firmware to existing firmware catalog
# The star on the src dir is used to make rsync not try to create the dest dir.
rsync -q --ignore-existing -av "$SRC_DIR"/*  "$DEST_DIR/"

