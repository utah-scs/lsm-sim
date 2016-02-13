#!/bin/bash

OUTPUT="/home/drushton/projects/lsm-results/"
TIME=$(date +"%m_%d_%y_%T") 
LOG=$OUTPUT/$TIME.log

# Policy ids in .compute.cpp
SHADOWLRU=0
FIFO=1
LRU=2
SLAB=3
SHADOWSLAB=4

# Redirect stdout & stderr to log.
exec  > >(tee -a $LOG)
exec 2> >(tee -a $LOG >&2)

# Runs simulation for shadowlru.
run_glru () {

  INPUT=$1
  APPID=${INPUT##*/} 
  APP=${APPID/app}
  POLICY="gLRU"
  TIME=$(date +"%m_%d_%y_%T") 
  FILENAME=$TIME.$APPID.$POLICY

  printf "%s   %s  %s\n" $TIME $APPID $POLICY
  ../compute -p $SHADOWLRU -a $APP -f $INPUT
  mv shadowlru-size-curve.data $OUTPUT$FILENAME."size-curve.data"
  mv shadowlru-position-curve.data $OUTPUT$FILENAME."position-curve.data"
  echo
}

# Runs simulation for shadowslab for each growth factor.
run_slab () {

  INPUT=$1
  APPID=${INPUT##*/}  
  APP=${APPID/app}
  POLICY="sSLB"
  TIME=$(date +"%m_%d_%y_%T")
  APPEND="size-curve.data"
  
  # Run with default 1.25 growth factor. 
  FACTOR=1.25 
  FILENAME=$TIME.$APPID.$POLICY.$FACTOR".size-curve.data"
  printf "%s   %s   %s   %s\n" $TIME $APPID $POLICY $FACTOR
  ../compute -p $SHADOWSLAB -a $APP -f $INPUT -g $FACTOR
  mv shadowslab-size-curve.data $OUTPUT$FILENAME
  echo

  # Run with default 1.07 growth factor (Facebook paper). 
  # FACTOR=1.07
  # FILENAME=$TIME.$APPID.$POLICY.$FACTOR"size-curve.data"
  # printf "%s   %s   %s   %s\n" $TIME $APPID $POLICY $FACTOR
  # ../compute -p $SHADOWSLAB -a $APP -f $INPUT -g $FACTOR
  # mv shadowslab-size-curve.data $OUTPUT$FILENAME
  # echo
}


# ... add funcs for other policies here


# Main routine, 
# For all input traces matching app**, run the following simulations.
echo "date/time(start)  app ID  policy"
echo "---------------------------------"
TRACES=/mnt/data/projects/lsm-traces/*;
#TRACES=~/test_traces/*;
for f in $TRACES
do
  if [[ "${f/app20}" != "$f" ]];  # TEMP, CHANGE THIS FOR ALL APPS!!!
  then
    run_glru $f
    echo 
    run_slab $f
    echo
  fi
done


# Output final time so we know everything finished.
TIME=$(date +"%m_%d_%T")
echo $TIME "done."

