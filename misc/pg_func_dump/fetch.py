#!/usr/bin/python

import sys, os, re

def main():
    input = open("../../kcdpg/lib.c")
    for line in input.readlines():
	match = re.compile('^KCDPG_QUERY_STRUCT\((\w+)\)').match(line)
        if match:
            name = match.group(1)
            print ("CREATE OR REPLACE FUNCTION %s" + "(bytea) RETURNS bytea AS 'libkcdpg', " +\
                   "'kcdpg_%s' LANGUAGE C STRICT;") % (name, name)
            
            
    
main()

