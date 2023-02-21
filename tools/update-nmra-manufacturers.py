#!/usr/bin/env python3
# encoding=utf8

from urllib.parse import urljoin
from urllib.request import urlopen

import hashlib
import json
import os.path
import re
import subprocess

standards_url = 'https://www.nmra.org/index-nmra-standards-and-recommended-practices/'
appendix_filename = 'docs/NMRA/appendix_a_s-9_2_2_5.pdf'
appendix_title = 'S-9.2.2, Appendix A'
database_filename = 'lmrs/core/data/manufacturers.json'

def find_manufacturers_list():
    """
    Find the URL of the manufacturers appendix on the NMRA website.
    """

    print('Searching manufacturers list...')
    response = urlopen(standards_url).read().decode('utf-8')
    match = re.search(r'<a[^>]+href="([^"]+)"[^>]*>%s'
                      % appendix_title, response)

    if not match:
        raise SystemExit(f'Could not find "{appendix_title}". Aborting.')

    return urljoin(standards_url, match.group(1))


def update_manufacturers_pdf(appendix_url, filename):
    """
    Download latest version of the manufacturers appendix from NMRA website.
    """

    print('Downloading manufacturers list...')
    new_appendix = urlopen(appendix_url).read()
    new_digest = hashlib.sha1(new_appendix).digest()
    old_digest = hashlib.sha1(open(filename, 'rb').read()).digest()

    if new_digest != old_digest:
        print('Updating local copy of manufacturers list...')
        open(filename, 'wb').write(new_appendix)


def generate_manufacturers_json(filename):
    """
    Generates the manufacturers JSON from PDF
    """

    # Convert the appendex PDF to plain text
    pdftotext = ['pdftotext', '-layout', filename, '-']
    pdftotext = subprocess.run(pdftotext, capture_output=True)

    within_table1 = False
    manufacturers = []
    revision = None

    # Process the plain text line by line
    for line in pdftotext.stdout.decode('utf-8').split('\n'):
        line = line.strip()

        # Extract revision number from plain text
        if revision is None:
            match = re.search(r'Revised:\s+(\d{1,2}\s+\w+\s+\d{4})', line)

            if match:
                revision = match.group(1)

        # Ignore all text outside of table 1
        if line.startswith('Table 1'):
            within_table1 = True
            continue
        elif line.startswith('Table 2'):
            break

        # Ignore headers and footers
        if not line or re.match(r'\s*(S-9\.2\.2|Manufacturer\s+Binary)', line):
            continue

        match = re.match(r'(.*\S)\s+[0-9]+\s+0[xX][0-9A-F]{2}\s+'
                         + r'(\d+)\s+([A-Z/-]+)', line)

        if match:
            # Store collected manufacturer record
            name, id, countries = match.groups()

            manufacturers.append({
                'id': int(id),
                'name': name,
                'countries': (countries != ' ' and countries.split('/') or []),
            })

        elif within_table1:
            # Deal with manufacturer names spaning multiple lines
            manufacturers[-1]['name'] += ' ' + line

    # Produce the actual document
    document = {
        '$origin': standards_url,
        '$title': 'NMRA ' + appendix_title,
    }

    if revision:
        document['$revision'] = revision

    document['manufacturers'] = manufacturers

    json.dump(document, open(database_filename, 'w'), indent=2)


if __name__ == '__main__':
    appendix_filename = os.path.join(__file__, '../..', appendix_filename)
    appendix_filename = os.path.normpath(appendix_filename)

    database_filename = os.path.join(__file__, '../..', database_filename)
    database_filename = os.path.normpath(database_filename)

    if False:
        update_manufacturers_pdf(find_manufacturers_list(), appendix_filename)

    generate_manufacturers_json(appendix_filename)
