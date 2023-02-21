#!/usr/bin/env python3

from re import match
from sys import argv

esu = open(argv[1], 'r')
z21 = open(argv[2], 'w')

for line in esu:
    for entry in line.split(','):
        m = match(r'CV(\d+) =\s+(\d+)', entry)
        if m:
            z21.write(m[1] + ';' + m[2] + ';\n')
