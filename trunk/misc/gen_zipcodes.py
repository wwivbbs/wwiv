#!/usr/bin/env python
# vim: ts=4 sw=4 sts=4 et
#
# gen_zipfiles.py
#
# read csv input from unitedstateszipcodes.org and output data
# files in a format useful by WWIV

# row[0] = zip
# row[2] = City
# row[5] = State

import csv
import string
import ConfigParser
from optparse import OptionParser


def parse_zip_data(infile):
    if debug:
        print 'parsing %s' % (infile)
    zipcodes = {}
    cities = {}
    with open(infile, 'rb') as csvfile:
        zipdb_reader = csv.reader(csvfile, delimiter=',', quotechar='"')
        for row in zipdb_reader:
            for zcgroup in range(10):
                zipgroup = str(zcgroup)
                if row[0].startswith(zipgroup):
                    zipcodes.setdefault(zipgroup,{})
                    zipcodes[zipgroup][row[0]] = {'state':row[5],'city':row[2]}

                    citygroup = row[2][0].lower()
                    cities.setdefault(citygroup,{})
                    citykey = '%s--%s' % (row[2].lower(),row[5].lower())
                    cities[citygroup].setdefault(citykey,{ 'city':row[2],
                                                            'state':row[5],
                                                            'zipcodes':[]})
                    cities[citygroup][citykey]['zipcodes'].append(row[0])
                    break
    return zipcodes,cities


def write_zip_data(zc):
    if debug:
        print 'zip code groups: ', sorted(zc)
    for group in sorted(zc):
        with open('zip%s.dat' % (group), 'w') as zip_output:
            for zipcode in sorted(zc[group]):
                zip_output.write( '%s\t%s\t%s\r\n' % (zipcode, 
                                      zc[group][zipcode]['state'], 
                                      zc[group][zipcode]['city']))


def write_city_data(cities):
    if debug:
        print 'city groups: ', sorted(cities)
    for group in sorted(cities):
        with open('city-%s.dat' % (group), 'w') as city_output:
            for ckey in sorted(cities[group]):
                for zipcode in sorted(cities[group][ckey]['zipcodes']):
                    city_output.write('%s %s %s\r\n' % (cities[group][ckey]['city'], 
                                                   cities[group][ckey]['state'], 
                                                   zipcode))
            


if __name__ == '__main__':
    parser = OptionParser()
    parser.add_option("-d", "--debug", action="store_true", dest="debug", help="Turn on Debug mode")
    parser.set_defaults(debug=False)
    (options, args) = parser.parse_args()

    debug = options.debug

    if args:
        if debug:
            print 'args list: ', args
        zipcodes,cities = parse_zip_data(args[0])
        write_zip_data(zipcodes)
        write_city_data(cities)
    else:
        parser.print_help()
        parser.error("you need to define an input file to parse")

