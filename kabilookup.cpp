/* kabi-lookup.cpp - search the graph generated by kabi-parser for symbols
 *                   given by the user
 *
 * Copyright (C) 2015  Red Hat Inc.
 * Tony Camuso <tcamuso@redhat.com>
 *
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
 *
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "checksum.h"
#include "kabilookup.h"

using namespace std;
using boost::format;

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

string lookup::get_helptext()
{
	return "\
\n\
kabi-lookup -[qw] -c|d|e|s symbol [-f file-list]\n\
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
    -q          - \"Quiet\" option limits the amount of output. \n\
    -w          - whole words only, default is \"match any and all\" \n\
    -f filelist - Optional list of data files that were created by kabi-parser. \n\
                  The default list created by running kabi-data.sh is \n\
		  \"./redhat/kabi/parser/kabi-files.list\" relative to the \n\
                  top of the kernel tree. \n\
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
}

int lookup::run()
{
	ifstream ifs(m_filelist.c_str());

	if(!ifs.is_open()) {
		cout << "Cannot open file: " << m_filelist << endl;
		exit(EXE_NOFILE);
	}

	while (getline(ifs, m_datafile)) {

		if (!(m_flags & KB_COUNT)) {
			cout << m_datafile << "\r";
			cout.flush();
		}

		m_errindex = execute(m_datafile);

		if (!(m_flags & KB_COUNT)) {
			cout << "\33[2K\r";
			cout.flush();
		}

		if (m_isfound && (m_flags & KB_WHOLE_WORD)
			      && ((m_flags & KB_EXPORTS)
			      ||  (m_flags & KB_DECL)))
			break;
	}

	cout << endl;
	m_err.print_cmd_errmsg(m_errindex, m_declstr, m_filelist);
	return(m_errindex);
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
	if (m_flags & KB_QUIET)
		m_flags &= ~KB_VERBOSE;

	return !(count_bits(m_flags & m_exemask) > 1);
}

int lookup::process_args(int argc, char **argv)
{
	int argindex = 0;

	// skip over argv[0], which is the invocation of this app.
	++argv; --argc;

	if(!argc)
		return EXE_ARG2SML;

	m_flags = KB_VERBOSE;
	m_flags |= m_opts.get_options(&argindex, &argv[0], m_declstr, m_filelist);

	if (m_flags < 0 || !check_flags())
		return EXE_BADFORM;

	return EXE_OK;
}

int lookup::execute(string datafile)
{
	if(kb_read_dnodemap(datafile, m_dnmap) != 0)
		return EXE_NOFILE;

	switch (m_flags & m_exemask) {
	case KB_COUNT   : return exe_count();
	//case KB_DECL    : return exe_decl();
	case KB_EXPORTS : return exe_exports();
	case KB_STRUCT  : return exe_struct();
	}
	return 0;
}

int lookup::exe_count()
{
	if (m_opts.kb_flags & KB_WHOLE_WORD) {
		m_crc = raw_crc32(m_declstr.c_str());
		dnode* dn = kb_lookup_dnode(m_crc);
		m_count += dn ? dn->siblings.size() : 0;
	} else {
		for (auto it : m_dnmap) {
			dnode& dn = it.second;
			if (dn.decl.find(m_declstr) != string::npos)
				++m_count;
		}
	}

	cout << m_count << "\r";
	cout.flush();
	return m_count !=0 ? EXE_OK : EXE_NOTFOUND;
}

bool lookup::find_decl(dnode &dnr, string decl)
{
	m_crc = ::raw_crc32(decl.c_str());
	dnode* dn = kb_lookup_dnode(m_crc);

	if (dn) {
		dnr = *dn;
		return true;
	}
	return false;
}

int lookup::exe_struct()
{
	bool quiet = m_flags & KB_QUIET;

	if (m_opts.kb_flags & KB_WHOLE_WORD) {
		unsigned long crc = raw_crc32(m_declstr.c_str());
		dnode* dn = kb_lookup_dnode(crc);

		if (!dn)
			return EXE_NOTFOUND_SIMPLE;

		m_isfound = true;
		m_rowman.rows.clear();

		get_siblings_up(*dn);
		m_rowman.put_rows_from_back(quiet);

#if 0
	if (m_flags & KB_WHOLE_WORD) {
		unsigned long crc = raw_crc32(m_declstr.c_str());
		qnitpair_t range;
		range = m_qnodes.equal_range(crc);

		for_each (range.first, range.second,
			  [this, quiet](qnpair_t& lqp)
			  {
				qnode& qn = lqp.second;
				qn.crc = lqp.first;

				if (qn.flags & CTL_BACKPTR)
				//|| !(qn.flags & CTL_HASLIST))
					return;

				m_rowman.rows.clear();
				m_rowman.rows.reserve(qn.level);
				this->get_parents(qn);
				m_rowman.put_rows_from_back(quiet);
			  });
#endif
	}
#if 0
	else {
		for (auto it : m_qnodes) {
			qnode& qn = it.second;
			qn.crc = it.first;

			if (qn.sdecl.find(m_declstr) != string::npos) {
				m_rowman.rows.clear();
				m_rowman.rows.reserve(qn.level);
				this->get_parents(qn);
				m_rowman.put_rows_from_back(quiet);
			}
		}
	}
#endif

	return EXE_OK;
}

int lookup::get_parents(cnpair& cnp)
{
	cnode cn  = cnp.second;
	crc_t crc = cn.parent.second;

	if (!crc)
		return EXE_OK;

	dnode parentdn = *kb_lookup_dnode(crc);
	cnodemap siblings = parentdn.siblings;
	cniterator cnit;

	cnit = find_if(siblings.begin(), siblings.end(),
		[cn](cnpair& lcnp) {
			cnode lcn = lcnp.second;
			int nextlevelup = cn.level - 1;
			return ((lcn.level < 3) ||
				((lcn.level == nextlevelup) &&
				 (lcn.argument == cn.argument) &&
				 (lcn.function == cn.function)));
		});

	if (cnit == siblings.end())
		return EXE_OK;

	cnode parentcn = (*cnit).second;
	m_rowman.fill_row(parentdn, parentcn);
	this->get_parents(*cnit);
	return EXE_OK;
}

/*****************************************************************************
 * lookup::get_siblings(dnode& dn)
 * dn - reference to a dnode
 *
 * Walk the siblings cnodemap in the dnode to access each instance of the
 * symbol characterized by the dnode.
 *
 * First we need to divide the list by ancestry.
 * Then we order each of the unique ancestries by level, which is the depth
 * at which a given instance of a symbol (dnode) was instantiated.
 *
 */
int lookup::get_siblings_up(dnode& dn)
{
	crc_t prevarg = 0;
	crc_t prevfunc = 0;

	vector<cnodemap> siblings;
	cnodemap cnmap;
	bool firstpass = true;

	// Create a vector of cnodemap divided according to their ancestry,
	// and with each cnodemap indexed by level in the hierarchy in which
	// each cnode was instantiated.
	for (auto it : dn.siblings) {
		cnode cn = it.second;

		if (!firstpass &&
		   ((prevarg != cn.argument) || (prevfunc != cn.function))) {
			siblings.insert(siblings.begin(), cnmap);
			cnmap.clear();
		}

		cnmap.insert(cnmap.begin(), make_pair(cn.level, cn));
		prevarg = cn.argument;
		prevfunc = cn.function;
		firstpass = false;
	}

	// Loop through each siblings cnmap in reverse to get the the deepest
	// level at which this dnode is instantiated. Then, don't bother
	// traversing other siblings with the same ancestors, since we have
	// already done that from the deepest point.

	for (auto sibit : siblings) {
		cnodemap cnmap = sibit;
		cnpair cnp = *cnmap.rbegin();
		cnode cn = cnp.second;

		m_rowman.rows.reserve(cn.level);
		m_rowman.fill_row(dn, cn);
		get_parents(cnp);
	}

	return EXE_OK;
}

/*****************************************************************************
 * lookup::get_children(dnode& dn)
 *
 * Given references to a dnode and a cnode instance of it, walk the dnode's
 * children crcmap and gather the info on the children.
 * This is done reursively, until we've parsed all the children and all
 * their descendants.
 */
int lookup::get_children(dnode& dn)
{
	for (auto i : dn.children) {
		int order = i.first;
		crc_t crc = i.second;
		dnode dn = *kb_lookup_dnode(crc);
		cnodemap siblings = dn.siblings;
		cnode cn = siblings[order];

		m_rowman.fill_row(dn, cn);

		if (cn.flags & CTL_BACKPTR)
			continue;

		get_children(dn);
	}
	return EXE_OK;
}

/*****************************************************************************
 * lookup::get_siblings(dnode& dn)
 * dn - reference to a dnode
 *
 * Walk the siblings cnodemap in the dnode to access each instance of the
 * symbol characterized by the dnode.
 */
int lookup::get_siblings(dnode& dn)
{
	for (auto it : dn.siblings) {
		cnode cn = it.second;
		m_rowman.fill_row(dn, cn);
		get_children(dn);
	}
	return EXE_OK;
}

/*****************************************************************************
 * int lookup::exe_exports()
 *
 * Search the graph for exported symbols. If the whole word flag is set, then
 * the search will look for an exact match. In that case, it will find at most
 * one matching exported symbol.
 * If not, the code will search the graph for the string where ever it appears,
 * even as a substring, but only exported symbols are considered.
 */
int lookup::exe_exports()
{
	bool quiet = m_flags & KB_QUIET;

	if (m_opts.kb_flags & KB_WHOLE_WORD) {
		unsigned long crc = raw_crc32(m_declstr.c_str());
		dnode* dn = kb_lookup_dnode(crc);

		if (!dn)
			return EXE_NOTFOUND_SIMPLE;

		m_isfound = true;
		m_rowman.rows.clear();

		// If there is not exactly one sibling, then this is not an
		// exported symbol. Exports all exist in the same name space
		// and must be unique; there can only be one.
		if (dn->siblings.size() != 1)
			return EXE_NOTFOUND_SIMPLE;

		get_siblings(*dn);
		m_rowman.put_rows_from_front(quiet);

	} else {

		for (auto it : m_dnmap) {
			dnode& dn = it.second;

			if (dn.siblings.size() != 1)
				continue;

			cniterator cnit = dn.siblings.begin();
			cnode cn = cnit->second;

			if (!(cn.flags & CTL_EXPORTED))
				continue;

			if (cn.name.find(m_declstr) == string::npos)
				continue;

			m_rowman.rows.clear();
			get_siblings(dn);
			m_rowman.put_rows_from_front(quiet);
			m_isfound = true;
		}
	}

	return m_isfound ? EXE_OK : EXE_NOTFOUND_SIMPLE;
}

#if 0
int lookup::exe_decl()
{
	int count = 0;
	bool quiet = m_flags & KB_QUIET;
	qnode* qn = NULL;

	if (m_opts.kb_flags & KB_WHOLE_WORD) {
		unsigned long crc = raw_crc32(m_declstr.c_str());
		qniterator_t qnit;
		qnitpair_t range;
		range = m_qnodes.equal_range(crc);

		qnit = find_if (range.first, range.second,
			  [this, &count](qnpair_t& lqp)
			  {
				qnode& rqn = lqp.second;
				return ((rqn.children.size() > 0) &&
					(rqn.flags & CTL_HASLIST) &&
					(!(rqn.flags & CTL_BACKPTR)));
			  });

		qn = (qnit != range.second) ? &(*qnit).second : NULL;

		if (qn != NULL) {
			m_isfound = true;
			m_rowman.rows.clear();
			get_children_wide(*qn);
			m_rowman.put_rows_from_front_normalized(quiet);
		}

	} else {

		for (auto it : m_qnodes) {
			qnode& rqn = it.second;
			rqn.crc = it.first;

			if ((rqn.sdecl.find(m_declstr) != string::npos) &&
			    (rqn.children.size() > 0) &&
			    (rqn.flags & CTL_HASLIST) &&
			    (!(rqn.flags & CTL_BACKPTR))) {
				m_isfound = true;
				qn = &rqn;
				m_rowman.rows.clear();
				get_children_wide(*qn);
				m_rowman.put_rows_from_front_normalized(quiet);
			}
		}
	}
	return m_isfound ? EXE_OK : EXE_NOTFOUND_SIMPLE;
}
#endif

/************************************************
** main()
************************************************/
int main(int argc, char *argv[])
{
	lookup lu(argc, argv);
	return lu.run();
}

#if 0
int lookup::get_children_deep(dnode &parent, cnpair &cn)
{
	qnode& qn = *qn_lookup_qnode(&parent, cn.first, QN_DN);

	m_rowman.fill_row(qn);

	if (qn.children.size() == 0)
		return EXE_OK;

	cnodemap_t& cnmap = qn.children;

	for (auto it : cnmap)
		get_children_wide(*qn_lookup_qnode(&qn, it.first, QN_DN));

	return EXE_OK;
}
#endif

