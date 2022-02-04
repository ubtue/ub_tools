#include "DbConnection.h"
#include "IniFile.h"
#include "UBTools.h"

const std::string UB_TOOLS_DIRECTORY("/usr/local/ub_tools");
const std::string INSTALLER_DATA_DIRECTORY(UB_TOOLS_DIRECTORY + "/cpp/data/installer");

IniFile translations_ini_file(UBTools::GetTuelibPath() + "translations.conf");
const auto translations_ini_section(translations_ini_file.getSection("Database"));
const std::string ixtheo_database(translations_ini_section->getString("sql_database"));
const std::string ixtheo_username(translations_ini_section->getString("sql_username"));
const std::string ixtheo_password(translations_ini_section->getString("sql_password"));

[[noreturn]] void Usage() {
    ::Usage(
        "at the moment there are no parameters\n"
        "    used to test (modified) ixtheo.sql file import, e.g. for stored procedures installation\n");
}

int Main(int argc, char **argv) {
    if (argc > 1 or std::strcmp("dummy", argv[0]) == 0)
        Usage();
    DbConnection::MySQLImportFile(INSTALLER_DATA_DIRECTORY + "/ixtheo.sql", ixtheo_database, ixtheo_username, ixtheo_password);
    return EXIT_SUCCESS;
}