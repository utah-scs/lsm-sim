#!/bin/bash

run_glru () {

  POLICY="gLRU"
  TIME=$(date +"%m_%d_%y_%T")
  FILENAME=$TRACENAME.$POLICY.$TIME
  printf "Processing trace: %s policy: %s %s\n" $TRACENAME $POLICY $TIME
  # ./compute -p 3 -f $1 -o $FILENAME
}

run_shadow_slab () {

  POLICY="SSlb"
  TIME=$(date +"%m_%d_%y_%T")
  FILENAME=$TRACENAME.$POLICY.$TIME
  printf "Processing trace: %s policy: %s %s\n" $TRACENAME $POLICY $TIME
  # ./compute -p 4 -f $1 -o $FILENAME
}


# ... add rules for other policies here

# For all input traces
TRACES=./traces/*;
for f in $TRACES
do
  TRACENAME=${f##.*/} 
  run_glru $TRACENAME
  run_shadow_slab $TRACENAME 
done

TIME=$(date +"%m_%d_.%T")

echo $TIME "done."

