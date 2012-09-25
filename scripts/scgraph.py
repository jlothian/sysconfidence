#!/usr/bin/python
#  This file is part of SystemBurn.
#
#  Copyright (C) 2012, UT-Battelle, LLC.
#
#  This product includes software produced by UT-Battelle, LLC under Contract No. 
#  DE-AC05-00OR22725 with the Department of Energy. 
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the New BSD 3-clause software license (LICENSE). 
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
#  LICENSE for more details.
#
#  For more information please contact the SystemBurn developers at: 
#  systemburn-info@googlegroups.com


from optparse import OptionParser
from math import ceil, floor, log10
from sys import stderr,stdout
import re

# format of output files from sysconfidence:
# bin binbot bintop timer onNode1 oneNodePair onNode1Min onNodePairMin offNode1 offNodePair offNode1Min offNodePairMin
# 0   1      2      3     4       5           6          7             8        9           10          11

# if adding more constants, be sure to add an associated name below
BIN = 0
BINBOT = 1
BINTOP = 2
TIMER = 3
ONNODE_ONE = 4
ONNODE_PAIR = 5
ONNODE_ONEMIN = 6
ONNODE_PAIRMIN = 7
OFFNODE_ONE = 8
OFFNODE_PAIR = 9
OFFNODE_ONEMIN = 10
OFFNODE_PAIRMIN = 11
NUM_STATS = 12
DATAFILES = {"histogram": "global.HIST.0", "pdf":"global.PDF.0", "cdf":"global.CDF.0"}
# if adding more names, be sure to add an associated constant above
NAMES = ["", 
    "", 
    "", 
    "Timer", 
    "onNodeOneSided", 
    "onNodePairwise", 
    "onNodeOneSidedMinimum", 
    "onNodePairwiseMinimum",
    "offNodeOneSided",
    "offNodePairwise",
    "offNodeOneSideMinimum",
    "offNodePairwiseMinimum" ]
  


preamble = """#!/usr/bin/gnuplot
set macros
Node   = "using 3:6"
Nodemin= "using 3:7"
PW     = "using 3:10"
PWmin  = "using 3:12"
OS     = "using 3:9"
OSmin  = "using 3:11"
Timer  = "using 3:4"
set style line 1 lt rgb "green" lw 3 pt 0
set style line 2 lt rgb "red" lw 3 pt 0
set style line 3 lt rgb "blue" lw 3 pt 0
set style line 4 lt rgb "#FF00FF" lw 3 pt 0
set style line 5 lt rgb "cyan" lw 3 pt 0
set style line 6 lt rgb "greenyellow" lw 3 pt 0
set style line 7 lt rgb "slategray" lw 3 pt 0
set style line 8 lt rgb "#48D1CC" lw 3 pt 0
set style line 9 lt rgb "orangered" lw 3 pt 0
set style line 1 lt rgb "#BA55D3" lw 3 pt 0

set terminal png medium size 1600,1200
set xlabel "Latency (microseconds)"
"""

def min_max_with_index(condata,colnumber):
    """return a set of minimums and maximums for the given data set and column"""
    min_y = 1.0e99
    min_x = 1.0e99
    max_y = -1.0
    max_x = -1.0
    oldval = 0
    val=0

    for row in condata:
        oldval = val
        val = float(row[colnumber])
        if 0.0 < val < min_y:
            min_y = val
        if val > max_y:
            max_y = val

        if val != 0.0 and val != oldval:
            if row[BINBOT] < min_x:
                min_x = row[BINBOT]
            if row[BINTOP] > max_x:
                max_x = row[BINTOP]

# make sure we've got reasonable bounds
    if min_y == 1.0e99:
        min_y = 1.0e-6
        min_x = 0

    if max_x < 0.0:
        max_x = 0

    return (min_x, max_x, min_y, max_y)

class dataColumn(object):
    """represent the minimums and maximums for a column"""
    def __init__(self):
# x are our latencies, y our counts or probabilities
        self.xmin = 1.0e5
        self.xmax = -1.0
        self.ymin = 1.0e5
        self.ymax = -1.0

class dataFile(object):
    """encapsulates a data set and associated options"""
    def __init__(self, filename):
        self.columns = [] # create placeholders for our columns
        for i in range(NUM_STATS):
            self.columns.append(dataColumn())

        self.filename = filename

        try:
            self.fh = open(filename, "r")
        except Exception, e:
            print e

    def parse(self):
        """Read in the file, and put it into some happy data structures"""
        lines = self.fh.readlines()
        self.rows = []
        for line in lines:
            if re.match("^#", line):
                continue
            line = line.rstrip()
            line = line.lstrip()
            fields = re.split("[ \t]*", line)
            fields = [ float(f) for f in fields ]
            #print fields
            self.rows.append(fields)
            #print float(fields[2])
        #print first column
        for i in range(3,NUM_STATS):
            stats = min_max_with_index(self.rows, i)
            self.columns[i].xmin = stats[0]
            self.columns[i].xmax = stats[1]
            self.columns[i].ymin = stats[2]
            self.columns[i].ymax = stats[3]

class caseData(object):
    """a particular data case"""
    def __init__(self, casename):
        self.casename = casename
        self.datafiles = {}

    def load(self):
        for f in DATAFILES.keys():
            self.datafiles[f] = dataFile(self.casename+"/"+DATAFILES[f]);
            self.datafiles[f].parse()

def graphString(graphtype, graphnum, cases): # expects a list of cases
    """return a string representing the gnuplot command to
       create a graph for the given type (cdf, pdf, histogram)
       given a column to graph and the various different cases"""

    outfile = '"%s-%s.png"' % (graphtype, NAMES[graphnum])

    xmins = []
    xmaxs = []
    ymins = []
    ymaxs = []
    for c in cases:
        xmins.append(c.datafiles[graphtype].columns[graphnum].xmin)
        xmaxs.append(c.datafiles[graphtype].columns[graphnum].xmax)
        ymins.append(c.datafiles[graphtype].columns[graphnum].ymin)
        ymaxs.append(c.datafiles[graphtype].columns[graphnum].ymax)

    xmin = min(xmins)
    xmax = max(xmaxs)
    ymin = min(ymins)
    ymax = max(ymaxs)

    #stderr.write("%s: xmin: %e xmax: %e ymin: %e ymax: %e\n" % (outfile, xmin, xmax, ymin, ymax))

    if ymax == 0.0:
        if ymin == 0.0:
            ymax = 1.1
        else:
            ymax = 1.0



    if xmin == 0.0 and xmax == 0.0:
        xmax = 1.0


    if graphtype == "cdf":
        output = "unset logscale\nset logscale x\n"
# get some slightly better mins/maxes
        if xmin < 1.0e-6:
            #print "Min x for %s was %e, resetting\n" % ( outfile, xmin )
            xmin = 1.0e-6
        else:
            xmin = pow(10,floor(log10(xmin)))
        xmax = pow(10,ceil(log10(xmax)))
    elif graphtype == "pdf":
        output = "unset logscale\nset logscale xy\n"
        if xmin < 1.0e-6:
            #print "Min x for %s was %e, resetting\n" % ( outfile, xmin )
            xmin = 1.0e-6
        else:
            xmin = pow(10,floor(log10(xmin)))
        xmax = pow(10,ceil(log10(xmax)))
        if ymin < 1.0e-12:
            #print "Min y for %s was %e, resetting\n" % ( outfile, ymin )
            ymin = 1.0
        else:
            ymin = pow(10,floor(log10(ymin)))
        #print "type: %s num: %s ymax: %e\n" % (graphtype, graphnum, ymax)
        ymax = pow(10,ceil(log10(ymax)))
    else:
        output = "unset logscale\n"

   
    #stderr.write("  FINAL: %s: xmin: %e xmax: %e ymin: %e ymax: %e\n\n" % (outfile, xmin, xmax, ymin, ymax))

    output += 'set output %s\nset xr [%e:%e]\nset yr [%e:%e]\n' % ( outfile, xmin, xmax, ymin, ymax) 
    output += 'set title "Confidence %s Latency Observations"' % (NAMES[graphnum])
    output += '\nplot \\\n'
    ls = 1
    extrachars = ""
    for c in cases:
        output += '%s   "%s" using %d:%d title "%s" with linespoints ls %d' % ( extrachars, c.datafiles[graphtype].filename, BINTOP+1,graphnum+1,c.casename, ls)
        ls += 1
        extrachars = ", \\\n"
    
    output += "\n"
    return output



def main():

    cases = []
    parser = OptionParser()
    parser.add_option("-o", "--output", dest="outfile", help="output filename")
    (options, args) = parser.parse_args()

    for arg in args:
        cases.append(caseData(arg))

    # load everything
    for c in cases:
        c.load()

    output = preamble

    for i in range(3,12):
        output += graphString("pdf", i, cases)
        output += graphString("cdf", i, cases)
        output += graphString("histogram", i, cases)
    if options.outfile:
        fh = open(options.outfile, "w")
    else:
        fh = stdout

    print >> fh, output


if __name__ == "__main__":
        main()
