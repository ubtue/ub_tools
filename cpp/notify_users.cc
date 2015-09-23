/** \brief IxTheo utility to inform subscribed users of changes in monitored queries etc.
 *  \author Dr. Johannes Ruscheinski (johannes.ruscheinski@uni-tuebingen.de)
 *
 *  \copyright 2015 Universitätsbiblothek Tübingen.  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
A typical config file for this program looks like this:

user     = "root"
passwd   = "???"
database = "vufind"
*/

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <Compiler.h>
#include <DbConnection.h>
#include <DbResultSet.h>
#include <DbRow.h>
#include <IniFile.h>
#include <PHPUtil.h>
#include <util.h>


void Usage() {
    std::cerr << "usage: " << ::progname << " ini_file_path\n";
    std::exit(EXIT_FAILURE);
}


class SearchObject {
    std::vector<std::string> search_terms_;
    std::vector<std::string> filters_;
public:
    explicit SearchObject(const PHPUtil::Object &minSO);

    const std::vector<std::string> &getSearchTerms() const { return search_terms_; }
    const std::vector<std::string> &getFilters() const { return filters_; }
};


void StringArrayToVector(const PHPUtil::Array &array, std::vector<std::string> * const v) {
    v->clear();

    for (auto index_and_value(array.cbegin()); index_and_value != array.cend(); ++index_and_value) {
	const PHPUtil::String * const php_string(reinterpret_cast<PHPUtil::String *>(index_and_value->second.get()));
	assert(php_string != nullptr);
	v->emplace_back(php_string->getValue());
    }
}


SearchObject::SearchObject(const PHPUtil::Object &minSO) {
    if (unlikely(minSO.getClass() != "minSO"))
	Error("in SearchObject::SearchObject: expected instance of PHP class \"minSo\", found \""
	      + minSO.getClass() + "\" instead!");

    StringArrayToVector(reinterpret_cast<const PHPUtil::Array &>(minSO["t"]), &search_terms_);
    StringArrayToVector(reinterpret_cast<const PHPUtil::Array &>(minSO["f"]), &filters_);
}


/** \return true = we need to notify the user that something has changed that they would like to know about */
bool ProcessUser(const std::string &user_id, const std::string &/*email_address*/, DbConnection * const connection) {
    const std::string query("SELECT search_object FROM search WHERE user_id=" + user_id);
    if (not connection->query(query))
	Error("Query failed: \"" + query + "\" (" + connection->getLastErrorMessage() + ")!");

    DbResultSet result_set(connection->getLastResultSet());
    DbRow row;
    while (row = result_set.getNextRow()) {
	std::shared_ptr<PHPUtil::DataType> deserialised_object(PHPUtil::DeserialisePHPObject(row[0]));
	const PHPUtil::Object * const minSO(reinterpret_cast<PHPUtil::Object *>(deserialised_object.get()));
	if (unlikely(minSO == nullptr))
	    Error("unable to downcast a PHPUtil::DataType pointer to a PHPUtil::Object pointer!");

	const SearchObject search_object(*minSO);
    }

    return false;
}


struct UserIdAndEmail {
    const std::string user_id_;
    const std::string email_;
public:
    UserIdAndEmail(const std::string &user_id, const std::string &email): user_id_(user_id), email_(email) { }
};


int main(int argc, char *argv[]) {
    ::progname = argv[0];

    if (argc != 2)
	Usage();

    try {
	const IniFile ini_file(argv[1]);
	const std::string user(ini_file.getString("", "user"));
	const std::string passwd(ini_file.getString("", "passwd"));
	const std::string db(ini_file.getString("", "database"));

	const std::string query("SELECT id,email FROM user");
	DbConnection connection(db, user, passwd);
	if (not connection.query(query))
	    Error("Query failed: \"" + query + "\" (" + connection.getLastErrorMessage() + ")!");

	DbResultSet result_set(connection.getLastResultSet());
	DbRow row;
	while (row = result_set.getNextRow()) {
	    const std::string id(row[0]);
	    const std::string email_address(row[1]);
	    if (not ProcessUser(id, email_address, &connection))
		Error("Failed to process user w/ ID: " + id);
	}
    } catch (const std::exception &x) {
	Error("caught exception: " + std::string(x.what()));
    }
}
