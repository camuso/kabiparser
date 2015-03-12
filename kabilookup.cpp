/* kabi-lookup.cpp - search the graph generated by kabi-parser for symbols
 *                   given by the user
 *
 * Copyright (C) 2015  Red Hat Inc.
 * Tony Camuso <tcamuso@redhat.com>
 *
 ********************************************************************************
 * This is free software. You can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *******************************************************************************
 *
 */

#include <vector>
#include <cstring>
#include <fstream>

#include "kabilookup.h"

//#define NDEBUG
#if !defined(NDEBUG)
#define DBG(x) x
#define RUN(x)
#define prdbg(fmt, ...) \
do { \
       printf(fmt, ##__VA_ARGS__); \
} while (0)
#else
#define DBG(x)
#define RUN(x) x
#define prdbg(fmt, ...)
#endif


using namespace std;
using namespace kabilookup;

string lookup::get_helptext()
{
	return "\
\n\
kabi-lookup -[vw] -c|d|e|s symbol [-b datafile]\n\
    Searches a kabi database for symbols. The results of the search \n\
    are printed to stdout and indented hierarchically.\n\
\n\
    -c symbol   - Counts the instances of the symbol in the kabi tree. \n\
    -s symbol   - Prints to stdout every exported function that is implicitly \n\
                  or explicitly affected by the symbol. In verbose mode, the \n\
                  chain from the exported function to the symbol is printed.\n\
                  It is advisable to use \"-c symbol\" first. \n\
    -e symbol   - Specific to EXPORTED functions. Prints the function, \n\
                  and its argument list. With the -v verbose switch, it \n\
                  will print all the descendants of nonscalar arguments. \n\
    -d symbol   - Seeks a data structure and prints its members to stdout. \n\
                  The -v switch prints descendants of nonscalar members. \n\
    -v          - verbose lists all descendants of a symbol. \n\
    -w          - whole words only, default is \"match any and all\" \n\
    -f datafile - Optional data file. Must be generated using kabi-parser. \n\
                  The default is \"../kabi-data.dat\" relative to top of \n\
                  kernel tree. \n\
    --no-dups   - A structure can appear more than once in the nest of an \n\
                  exported function, for example a pointer back to itself as\n\
                  parent to one of its descendants. This switch limits the \n\
                  appearance of a symbol\'s tree to just once. \n\
    -h  this help message.\n";
}

lookup::lookup(int argc, char **argv)
{
	m_err.init(argc, argv);

	if ((m_errindex = process_args(argc, argv))) {
		m_err.print_cmd_errmsg(m_errindex, m_declstr, m_datafile);
		exit(m_errindex);
	}

	if ((m_errindex = execute())) {
		m_err.print_cmd_errmsg(m_errindex, m_declstr, m_datafile);
		exit(m_errindex);
	}
}

int lookup::count_bits(unsigned mask)
{
	int count = 0;

	do {
		count += mask & 1;
	} while (mask >>= 1);

	return count;
}

// Check for mutually exclusive flags.
bool lookup::check_flags()
{
	if (count_bits(m_flags & m_exemask) > 1)
		return false;

	return true;
}

int lookup::process_args(int argc, char **argv)
{
	int argindex = 0;

	// skip over argv[0], which is the invocation of this app.
	++argv; --argc;

	if(!argc)
		return EXE_ARG2SML;

	m_flags = m_opts.get_options(&argindex, &argv[0], m_declstr, m_datafile);

	if (!check_flags())
		return EXE_BADFORM;

	return EXE_OK;
}

int lookup::execute()
{
	kb_read_qlist(m_datafile, m_qnlist);
	qn_make_slist();
	find_decl();

	switch (m_flags & m_exemask) {
	case KB_COUNT   : return exe_count();
//	case KB_DECL    : return exe_decl();
//	case KB_EXPORTS : return exe_exports();
	case KB_STRUCT  : return exe_struct();
	}
	return 0;
}

#include <boost/format.hpp>
using boost::format;

qnode *lookup::find_decl()
{
	for (unsigned i = 0; i < m_qnodes.size(); ++i) {
		if (!(m_qn = &m_qnodes[i])->sdecl.find(m_declstr))
			return &m_qnodes[i];
	}
	return NULL;
}

int lookup::get_decl_list(std::vector<qnode> &retlist)
{
	for (unsigned i = 0; i < m_qnodes.size(); ++i) {
		if (!(m_qn = &m_qnodes[i])->sdecl.compare
				(0, m_declstr.length(), m_declstr))
			retlist.push_back(*m_qn);
	}
	DBG(cout << format("\"%s\" size: %d\n") % m_declstr %  m_declstr.size();)

	return retlist.size();
}

int lookup::exe_count()
{
	DBG(cout << format("%08x %08x %s\n") \
	    % m_qn->cn->crc % m_qn->flags % m_qn->sdecl;)
	cout << m_qn->parents.size() << endl;

	return EXE_OK;
}

void lookup::fill_row(const qnode *qn, int level)
{
	row r;
	r.level = level;
	r.flags = qn->flags;
	r.file = qn->sfile;
	r.decl = qn->sdecl;
	r.name = qn->sname;
	m_rows.push_back(r);
}


string &lookup::pad_out(int padsize)
{
	static string out;
	out.clear();
	while(padsize--) out += " ";
	return out;
}

void lookup::put_row(row &r)
{
	switch (r.level) {
	case LVL_EXPORTED:
		cout << "FILE: " << r.file << endl
		     << " EXPORTED: " << r.decl << r.name << endl;
		break;
	case LVL_ARG:
		cout << ((r.flags & CTL_RETURN) ? "  RETURN: " : "  ARG: ");
		cout << r.decl << r.name << endl;
		break;
	default:
		cout << pad_out(r.level) << r.decl << r.name << endl;
		break;
	}
}

void lookup::put_rows()
{
	unsigned size = m_rows.size();
	for (unsigned i = 0; i < size; ++i) {
		row r = m_rows.back();
		put_row(r);
		m_rows.pop_back();
	}
}

int lookup::get_parents_deep(qnode *qn, int level)
{
	vector<cnode> cnlist = qn->parents;

	// The level passed as a parameter is the level at which the parent
	// of this symbol appears in the hierarchy. The correct level for
	// this symbol is one higher than that.
	fill_row(qn, level + 1);

	for (unsigned k = 0; k < cnlist.size(); ++k) {
		qn = qn_lookup_crc(cnlist[k].crc);

		// The next parent will have a level one less than that of
		// the symbol we just looked up (qn).
		if (qn->parents[k].level == level - 1) {
			get_parents_deep(qn, level - 1);
			return EXE_OK;
		}
	}
	return EXE_OK;
}

int lookup::get_parents_wide()
{
	vector<cnode> cnlist = m_qn->parents;

	for (unsigned j = 0; j < cnlist.size(); ++j) {
		// This is the level at which the parent appears in the
		// hierarchy.
		int level = cnlist[j].level;
		unsigned long crc = cnlist[j].crc;

		// The level of the base symbol is one higher than that
		// of its parent.
		m_rows.clear();
		m_rows.reserve(level + 1);
		fill_row(m_qn, level + 1);

		qnode *qn = qn_lookup_crc(crc);
		get_parents_deep(qn, level - 1);
		put_rows();
	}
	return EXE_OK;
}

int lookup::exe_struct()
{
	if (m_qn->parents.size() == 0) {
		m_rows.clear();
		fill_row(m_qn, 0);
		put_rows();
		return EXE_OK;
	}

	get_parents_wide();
	return EXE_OK;
}

/************************************************
** main()
************************************************/
int main(int argc, char *argv[])
{
	lookup lu(argc, argv);
}
