#!/usr/bin/python

import sys
import os

params=["vncreflector", "-g", "/var/log/vncreflector/vncreflector." + str(os.getpid()) + ".log", "-T", "5", "-Q", "5"]

i=1
while i < len(sys.argv):
	if sys.argv[i] == '-T':
		i = i + 1
	else:
		params += [sys.argv[i]]
	i = i + 1

os.execv("/usr/bin/vncreflector.bin", params)
