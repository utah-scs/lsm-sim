#!/bin/bash

# This script prints the following stats for each application trace.
# 1. Total number of records.
# 2. The unique number of records accessed.
# 3. The percentage of operation type.
# 4. The average object size.

apps=(1 2 3 5 6 7 8 10 11 13 18 19 20 23 29 31 53 59 94 227)
trace_dir="/mnt/data/projects/lsm-traces"

total() {
	echo "App  #Requests"
	for app in ${apps[@]}
	do
		total=$(cat $trace_dir/app$app | wc -l)
		echo app$app $total
	done
}

unique() {
	echo "App  #Unique_requests"
	for app in ${apps[@]}
	do
		count=$(cut -d ',' -f 6 $trace_dir/app$app | sort| uniq -c | wc -l)
		echo app$app $count
	done
}

percent() {
	echo "App	get%		put%	delete%		add%	incr%	stats%	other%"
	for app in ${apps[@]}
	do
		get=0.0
		put=0.0
		delete=0.0
		add=0.0
		incr=0.0
		stats=0.0
		other=0.0
		cut -d ',' -f 3 $trace_dir/app$app | sort | uniq -c > temp
		sum=$(awk 'BEGIN {sum = 0 } {sum += $1 } END {print sum}' temp)
		while IFS=" " read -r f1 f2
		do
			case "$f2" in
				1)
					get=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
				2)
					put=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
				3)
					delete=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
				4)
					add=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
				5)
					incr=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
				6)
					stats=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
				7)
					other=$(echo "scale=6; $f1 / $sum"*100 | bc -l)
					;;
			esac
		done < temp
		echo "app$app	$get	$put	$delete	$add	$incr	$stats	$other"
		rm -f temp
	done
}

avgsize() {
	echo "App  Avg_ObjSize"
	for app in ${apps[@]}
	do
		size=$(awk 'BEGIN { FS = ","; sum=0; count =0 } {if ($5 > 0) { sum+=$5; count+=1 }} END {print sum/count}' $trace_dir/app$app)
		echo app$app $size
	done
}

display_help() {
	echo "Usage: $0 [option] app_number(optional)">&2
	echo
	echo "--total		Print the total number of records for each application."
	echo "--unique	Print the unique number of records for each application."
	echo "--percent	Print percentage of operations(get/put etc) for each application."
	echo "--avgsize	Print average object size for each application."
	echo "--help		Print the help."
	echo
	echo "Example:./stats.sh --total 1"
	echo "Example:./stats.sh --total"
	echo
	exit 1
}

if [ $# -eq 2 ]; then
	apps=($2)
fi

case "$1" in
    --total)
	total # calling function total()
	;;
    --unique)
	unique # calling function unique()
	;;
    --percent)
	percent
	;;
    --avgsize)
	avgsize
	;;
    --help)
        display_help
	;;
    *)
	display_help
esac
