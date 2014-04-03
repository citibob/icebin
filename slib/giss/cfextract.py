import xml.dom.minidom
from xml.dom.minidom import Node
import os.path
import urllib2
import sys

# Extract only names we will use
extract_cf = {
	'land_ice_surface_specific_mass_balance_flux',
	"surface_downward_latent_heat_flux",
	"surface_temperature",
	"surface_downward_sensible_heat_flux"}

ofname = 'cfnames_data.cpp'

def get_val(entry, sub) :
	try :
		return entry.getElementsByTagName(sub)[0].childNodes[0].nodeValue
	except :
		return ''


def string_to_c(s, max_length = 140, unicode=False):
    ret = []

    # Try to split on whitespace, not in the middle of a word.
    split_at_space_pos = max_length - 10
    if split_at_space_pos < 10:
        split_at_space_pos = None

    position = 0
    if unicode:
        position += 1
        ret.append('L')

    ret.append('"')
    position += 1
    for c in s:
        newline = False
        if c == "\n":
            to_add = "\\\n"
            newline = True
        elif ord(c) < 32 or 0x80 <= ord(c) <= 0xff:
            to_add = "\\x%02x" % ord(c)
        elif ord(c) > 0xff:
            if not unicode:
                raise ValueError, "string contains unicode character but unicode=False"
            to_add = "\\u%04x" % ord(c)
        elif "\\\"".find(c) != -1:
            to_add = "\\%c" % c
        else:
            to_add = c

        ret.append(to_add)
        position += len(to_add)
        if newline:
            position = 0

        if split_at_space_pos is not None and position >= split_at_space_pos and " \t".find(c) != -1:
            ret.append("\\\n")
            position = 0
        elif position >= max_length:
            ret.append("\\\n")
            position = 0

    ret.append('"')

    return "".join(ret)


# Get the file
cf_fname = "cf-standard-name-table.xml"

if not os.path.exists(cf_fname) :
	print 'Fetching cf-standard-name-table.xml...'
	remote = urllib2.urlopen('http://cf-pcmdi.llnl.gov/documents/cf-standard-names/standard-name-table/26/cf-standard-name-table.xml')
#	print remote.info()['Content-Disposition']
	local = open(cf_fname, 'w')
	local.write(remote.read())
	local.close()



fout = open(ofname, 'w')
fout.write("""/* This file is machine-generated by cfextract.py.  DO NOT MODIFY!
Contents are derived from the file cf-standard-name-table.xml */

#include <giss/cfnames.hpp>

namespace giss {
namespace cf {

std::map<std::string, giss::CFName> const &standard_name_table()
{

static std::map<std::string, giss::CFName> ret {

""")


dom = xml.dom.minidom.parse(cf_fname)
std = dom.getElementsByTagName('standard_name_table')[0]
entries=std.getElementsByTagName('entry')
for entry in entries:
	id = entry.getAttribute('id')
	if id not in extract_cf : continue

	canonical_units = get_val(entry, 'canonical_units')
	grib = get_val(entry, 'grib')
	amip = get_val(entry, 'ampi')
	description = get_val(entry, 'description')

#	print id
#	print canonical_units
#	print grib
#	print amip
#	print string_to_c(description)

	fout.write('{%s, {%s, %s, %s, %s, %s}},\n\n' % ( \
		string_to_c(id), \
		string_to_c(id), \
		string_to_c(description), \
		string_to_c(canonical_units), \
		string_to_c(grib), \
		string_to_c(amip)))


fout.write("""};
return ret;
}

}} // namespace
""")

#    alist=node.getElementsByTagName('Title')
#    for a in alist:
#        Title= a.firstChild.data
#        print Title
