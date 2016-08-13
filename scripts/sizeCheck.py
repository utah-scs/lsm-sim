#!/usr/bin/python
from __future__ import division
import sys
import collections
import math

def percentile(N, percent):
	k = (len(N)-1) * percent
	f = math.floor(k)
	c = math.ceil(k)
	if f == c:
		return N[int(k)]
	d0 = N[int(f)] * (c-k)
	d1 = N[int(c)] * (k-f)
	return int(d0+d1)

outputResults = sys.argv[1]
outputfile = open(outputResults, 'w+')

traceFileName = sys.argv[2]
tracefile = open(traceFileName, 'r')

appIdUser = sys.argv[3]

keySizeDict = {}

totalSizes = 0
numElements = 0

for line in tracefile:
	tokens = line.split(',')
	if (tokens[2] != "1"):
		continue
	appId = tokens[1]
	if (appId != appIdUser):
		print line
		continue
	keySize = int(tokens[3])
	valueSize = int(tokens[4])
	kid = int(tokens[5])	
	objSize = valueSize + keySize
	if (valueSize <= 0):
		continue
	if (objSize >= 1024 * 1024):
		print line
		continue
	if (kid not in keySizeDict):
		keySizeDict[kid] = objSize
		totalSizes += objSize
		numElements += 1


outputfile.write("# unique keys " + str(numElements) +"\n")
outputfile.write("sum sizes " + str(totalSizes) + "\n")
outputfile.write("Average " + str(totalSizes / numElements) + "\n")

sortedValues = sorted(keySizeDict.values());

outputfile.write("50% < " + str(percentile(sortedValues, 0.5)) + "\n")
outputfile.write("75% < " + str(percentile(sortedValues, 0.75)) + "\n")
outputfile.write("90% < " + str(percentile(sortedValues, 0.90)) + "\n")
outputfile.write("99% < " + str(percentile(sortedValues, 0.99)) + "\n")


#outputfile.write("keyID, size\n");
#for key,value in sorted(keySizeDict.iteritems()):
#	outputfile.write(str(key) +  "," + str(value))
#	outputfile.write("\n")
