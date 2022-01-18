#!/bin/bash
BASEDIR=`pwd`
DIR=$1

mkdir -p  $BASEDIR/Nofence/firmware

# Be sure to compare new firmware versions. If the new release has built the same FW as
# the old, the difference must be NONE. An exception is zip files generated with time
# stamps

changed_files=`diff -r --brief -x '*.zip' --from-file=$DIR/firmware/  $BASEDIR/Nofence/firmware/ | grep '^Files'`
changed_files_count=`echo -n "$changed_files" | wc -l`
if [ "$changed_files_count" -gt 0 ]
then
     echo "**  Error in firmare - unexpected diff"
     echo "$changed_files"
     exit 1
fi

# Copy over all new files from firmware to existing firmware catalog
rsync -q --ignore-existing   -av $DIR/firmware/  $BASEDIR/Nofence/firmware/


