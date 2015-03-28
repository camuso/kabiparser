#include <iostream>
#include <boost/format.hpp>
#include "qrow.h"
#include "rowman.h"

using namespace std;
using namespace boost;

rowman::rowman()
{
	dups.resize(LVL_COUNT);
}

bool rowman::set_dup(qrow &row)
{
	int duplevel = row.level >= LVL_NESTED ? LVL_NESTED : row.level;
	qrow& dup = dups.at(duplevel);

	if (dup == row)
		return false;

	dup = row;
	return true;
}

bool rowman::is_dup(qrow &row)
{
	int duplevel = row.level >= LVL_NESTED ? LVL_NESTED : row.level;
	qrow& dup = dups.at(duplevel);
	return (dup == row);
}

void rowman::fill_row(const qnode& qn)
{
	qrow r;
	r.level = qn.level;
	r.flags = qn.flags;
	r.decl = qn.sdecl;
	r.name = qn.sname;
	rows.push_back(r);
}

string &rowman::indent(int padsize)
{
	static string out;
	out.clear();
	while(padsize--) out += " ";
	return out;
}

void rowman::print_row(qrow& r, bool verbose)
{
	if (is_dup(r))
		return;

	switch (r.level) {
	case LVL_FILE:
		clear_dups();
		if (set_dup(r))
			cout << "FILE: " << r.decl << endl;
		break;
	case LVL_EXPORTED:
		if (set_dup(r))
			cout << " EXPORTED: " << r.decl << " "
			     << r.name << endl;
		break;
	case LVL_ARG:
		if (set_dup(r)) {
			cout << ((r.flags & CTL_RETURN) ?
					 "  RETURN: " : "  ARG: ");
			cout << r.decl << " " << r.name << endl;
		}
		break;
	default:

		if(!verbose && is_dup(dups[LVL_ARG]))
			return;

		if (set_dup(r) && verbose)
			cout << indent(r.level) << r.decl << " "
			     << r.name << endl;
		break;
	}
}

void rowman::print_row_normalized(qrow& r, bool verbose)
{
	if (!m_normalized) {
		m_normalized = true;
		m_normalized_level = r.level;
	}

	int current_level = r.level - m_normalized_level;

	if (!verbose && (current_level > 1))
		return;

	if (is_dup(r))
		return;

	if (set_dup(r)) {
		cout << indent(current_level) << r.decl;
		if ((current_level) > 0)
			cout << " " << r.name;
		cout << endl;
	}
}


void rowman::put_rows_from_back(bool verbose)
{
	cout << "\33[2K\r";

	for (auto it : rows) {
		qrow& r = rows.back();
		if (rows.size() == 1)
			print_row(r, true);
		print_row(r, verbose);
		rows.pop_back();
	}
	cout << endl;
}

void rowman::put_rows_from_front(bool verbose)
{
	cout << "\33[2K\r";

	for (auto it : rows) {
		if (it == rows.back())
			print_row(it, true);
		print_row(it, verbose);
	}

	cout << endl;
}

void rowman::put_rows_from_back_normalized(bool verbose)
{
	cout << "\33[2K\r";

	for (auto it : rows) {
		qrow& r = rows.back();
		print_row_normalized(r, verbose);
		rows.pop_back();
	}
	cout << endl;
}

void rowman::put_rows_from_front_normalized(bool verbose)
{
	cout << "\33[2K\r";

	for (auto it : rows)
		print_row_normalized(it, verbose);

	cout << endl;
}
