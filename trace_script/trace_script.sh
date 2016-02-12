#!/bin/bash

OUTPUT="/home/drushton/projects/lsm-results/"
TIME=$(date +"%m_%d_%y_%T") 
LOG=$OUTPUT/$TIME.log

exec  > >(tee -a $LOG)
exec 2> >(tee -a $LOG >&2)

run_glru () {

  INPUT=$1
  APPID=${INPUT##*/}  
  POLICY="gLRU"
  TIME=$(date +"%m_%d_%y_%T") 
  FILENAME=$TIME.$APPID.$POLICY
 
  printf "%s   %s  %s\n" $TIME $APPID $POLICY
  ../compute -p 0 -f $1 
  mv shadowlru-size-curve.data $OUTPUT$FILENAME."size-curve.data"
  mv shadowlru-position-curve.data $OUTPUT$FILENAME."position-curve.data"

}

run_slab () {

  INPUT=$1
  APPID=${INPUT##*/}  
  POLICY="sSLB"
  TIME=$(date +"%m_%d_%y_%T")
  APPEND="size-curve.data"
  FILENAME=$TIME.$APPID.$POLICY."size-curve.data"
  
  printf "%s   %s   %s\n" $TIME $APPID $POLICY
  ../compute -p 4 -f $1
  mv shadowslab-size-curve.data $OUTPUT$FILENAME
}


# ... add rules for other policies here

# For all input traces
echo "date/time(start)  app ID  policy"
echo "---------------------------------"
TRACES=/mnt/data/projects/lsm-traces/*;
#TRACES=~/test_traces/*;
for f in $TRACES
do
  if [[ "${f/app}" != "$f" ]];
  then
    run_glru $f
    echo 
    run_slab $f
    echo
  fi
done

TIME=$(date +"%m_%d_%T")

echo $TIME "done."

