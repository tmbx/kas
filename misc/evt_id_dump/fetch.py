#!/usr/bin/python

import sys, os, re

def main():
    input = open("../../common/kanp_core_defs.h")
    output = open("out.c", "wb")
    evt_list = []
    
    prog = "#include <stdio.h>\n\n"
    
    for line in input.readlines():
	prog += line
	
	match = re.compile('^#define (KANP_(?:EVT|CMD|RES)_\w+)').match(line)
	if match:
	    evt_list.append(match.group(1))
	
    prog += '\n\nint main()\n{\n'
    
    for evt in evt_list:
	prog += '\tprintf("' + evt + ' = %u\\n", ' + evt + ');\n'
    
    prog += '\n\treturn 0;\n}\n\n'
    
    output.write(prog)
    output.close()

main()
