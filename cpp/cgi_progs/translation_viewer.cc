// Generated using AI
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include "DbConnection.h"
#include "DbResultSet.h"
#include "DbRow.h"
#include "IniFile.h"
#include "JSON.h"
#include "UBTools.h"
#include "WebUtil.h"
#include "util.h"

namespace {

struct TranslationEntry {
    std::string ppn;
    std::string language;
    std::string keyword;
    std::string translation;
    std::string gnd_code;
};

DbResultSet ExecSqlAndReturnResultsOrDie(const std::string &select_statement, DbConnection * const db_connection) {
    db_connection->queryOrDie(select_statement);
    return db_connection->getLastResultSet();
}

const std::string CONF_FILE_PATH = (UBTools::GetTuelibPath() + "translations.conf");

void GetAdmissableTranslationLanguages(DbConnection &db_connection, std::set<std::string> * const admissible_translation_languages) {
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(R"SQL(
      SELECT DISTINCT language_code FROM keyword_translations WHERE language_code != 'ger' ORDER BY language_code
   )SQL",
                                                        &db_connection));
    while (const DbRow db_row = result_set.getNextRow())
        admissible_translation_languages->emplace(db_row["language_code"]);
}

void LoadTranslations(DbConnection &db_connection, const std::string &view_language, std::vector<TranslationEntry> * const translations) {
    DbResultSet result_set(ExecSqlAndReturnResultsOrDie(std::format(R"SQL(
    
    WITH keywords_newest AS (
        SELECT *
        FROM keyword_translations
        WHERE next_version_id IS NULL
    ),
    ppns AS (SELECT ppn FROM keyword_translations 
             WHERE language_code='ger' AND status='reliable' ORDER BY translation),
    result_set AS (SELECT * FROM keywords_newest WHERE ppn IN (SELECT ppn FROM ppns))
    SELECT l.ppn, k.translation AS keyword, l.translation, l.language_code, l.gnd_code  FROM 
           result_set AS l INNER JOIN result_set AS k ON k.language_code='ger' AND k.status='reliable' AND 
           k.ppn=l.ppn AND l.status IN ('reliable', 'new')  
           WHERE l.language_code IN ('{}') ORDER BY l.translation
    )SQL",
                                                                    view_language),
                                                        &db_connection));


    if (result_set.empty())
        return;


    while (const auto db_row = result_set.getNextRow()) {
        TranslationEntry entry;
        entry.ppn = db_row["ppn"];
        entry.language = db_row["language_code"];
        entry.keyword = db_row["keyword"];
        entry.translation = db_row["translation"];
        entry.gnd_code = db_row["gnd_code"];
        translations->emplace_back(std::move(entry));
    }
}


void EmitHttpHeader() {
    std::cout << "Content-Type: text/html; charset=UTF-8\r\n"
              << "X-Frame-Options: SAMEORIGIN\r\n"
              << "X-Content-Type-Options: nosniff\r\n"
              << "\r\n";
}


void EmitHtmlPrefix() {
    std::cout << R"HTML(
<!DOCTYPE html>
<html lang="en">

<head>

<meta charset="utf-8">

<title>Translation Viewer</title>

<link
 rel="stylesheet"
 href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.7/dist/css/bootstrap.min.css">
</link>
<style>

body {
    margin:20px;
}

.results {
    height:700px;
    overflow:auto;
}

.table-hover tbody tr:hover {
    cursor:pointer;
}

.details {
    border-left:1px solid #ddd;
    padding-left:20px;
}

</style>

</head>

<body>

<div id="app" class="container-fluid">

<div class="row">

<div class="col-md-8">

<h2>Translation Viewer</h2>

<input
 class="form-control mb-3"
 v-model="query"
 placeholder="Search keyword, translation, PPN or GND">

<div class="mb-2">

Showing {{ filtered.length }}
of {{ translations.length }} translations

</div>

<div class="results">

<table
 class="table table-sm table-striped table-hover">

<thead>

<tr>
<th>Translation</th>
<th>Keyword</th>
</tr>

</thead>

<tbody>

<tr
 v-for="item in visibleResults"
 @click="select(item)">

<td>{{ item.translation }}</td>
<td>{{ item.keyword }}</td>

</tr>

</tbody>

</table>

</div>

</div>

<div class="col-md-4 details">

<h4>Details</h4>

<div v-if="selected">

<table class="table table-bordered">

<tr>
<th>PPN</th>
<td>{{ selected.ppn }}</td>
</tr>

<tr>
<th>GND</th>
<td><a v-bind:href="'http://d-nb.info/gnd/' + selected.gnd" target="_blank">{{ selected.gnd }}</a></td>
</tr>


<tr>
<th>Language</th>
<td>{{ selected.language }}</td>
</tr>

<tr>
<th>Keyword</th>
<td>{{ selected.keyword }}</td>
</tr>

<tr>
<th>Translation</th>
<td>{{ selected.translation }}</td>
</tr>

</table>

</div>

<div v-else>

Select a translation.

</div>

</div>

</div>


<div class="d-flex align-items-center gap-2 mb-3">
 
    <button
        class="btn btn-sm btn-primary"
        @click="prevPage"
        :disabled="page <= 1">
        Previous
    </button>
 
    <span>
        Page {{ page }} of {{ pageCount }}
    </span>
 
    <button
        class="btn btn-sm btn-primary"
        @click="nextPage"
        :disabled="page >= pageCount">
        Next
    </button>
</div>


)HTML";
}


void EmitDataset(const std::vector<TranslationEntry> &translations) {
    std::cout << "<script>\n"
              << "window.TRANSLATIONS=[";

    bool first = true;

    for (const auto &t : translations) {
        if (!first)
            std::cout << ",";

        first = false;

        std::cout << "{" << "\"ppn\":\"" << JSON::EscapeString(t.ppn) << "\","

                  << "\"gnd\":\"" << JSON::EscapeString(t.gnd_code) << "\","

                  << "\"language\":\"" << JSON::EscapeString(t.language) << "\","

                  << "\"keyword\":\"" << JSON::EscapeString(t.keyword) << "\","

                  << "\"translation\":\"" << JSON::EscapeString(t.translation) << "\"" << "}";
    }

    std::cout << "];\n"
              << "</script>\n";
}


void EmitJavascript() {
    std::cout << R"HTML(

<script src="https://cdn.jsdelivr.net/npm/vue@3/dist/vue.global.prod.js"></script>
<script src="https://cdn.jsdelivr.net/npm/fuse.js@7/dist/fuse.min.js"></script>

<script>

const translations =
    window.TRANSLATIONS;

const fuse =
    new Fuse(
        translations,
        {
            keys:[
                {
                    name:"keyword",
                    weight:0.50
                },
                {
                    name:"translation",
                    weight:0.40
                },
                {
                    name:"ppn",
                    weight:0.10
                },
                {
                    name:"gnd",
                    weight:0.10
                }
            ],
            threshold:0.2,
            includeScore:true
        }
    );

Vue.createApp({

    data() {

        return {

            query:"",
            translations:
                translations,

            selected:null,
            page: 1,
            pageSize:200
        };
    },

    computed: {
        filtered() {
           const q = this.query.trim();
      
            if (!q)
                return this.translations;
      
            //
            // Exact PPN/GND lookup first
            //
            const exactMatches = this.translations.filter(t =>
                t.ppn === q ||
                t.gnd === q
            );
      
            if (exactMatches.length > 0) {
                console.log(
                  "Exact match",
                  exactMatches.length
                );
                return exactMatches;
            }
      
            //
            // Otherwise use Fuse fuzzy search
            //
            return fuse
                .search(q)
                .map(result => result.item);
        },  

        pageCount() {
            return Math.ceil(
                this.filtered.length / this.pageSize
            );
        },
     
        visibleResults() {
            const start =
                (this.page - 1) * this.pageSize;
     
            return this.filtered.slice(
                start,
                start + this.pageSize
            );
        }
    },

    watch: {
        query() {
            this.page = 1;
        }
    },

    methods: {

        select(item) {

            this.selected =
                item;
        },

        nextPage() {
            if (this.page < this.pageCount)
                this.page++;
        },
     
        prevPage() {
            if (this.page > 1)
                this.page--;
        }
    }

}).mount('#app');

</script>

</body>
</html>

)HTML";
}

} // end unnamed namespace

int Main(int argc, char *argv[]) {
    std::multimap<std::string, std::string> cgi_args;
    WebUtil::GetAllCgiArgs(&cgi_args, argc, argv);

    std::string view_language(WebUtil::GetCGIParameterOrDefault(cgi_args, "lang", "eng"));

    const IniFile ini_file(CONF_FILE_PATH);
    const std::string sql_database(ini_file.getString("Database", "sql_database"));
    const std::string sql_username(ini_file.getString("Database", "sql_username"));
    const std::string sql_password(ini_file.getString("Database", "sql_password"));
    DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));


    std::set<std::string> admissible_translation_languages;
    GetAdmissableTranslationLanguages(db_connection, &admissible_translation_languages);
    if (admissible_translation_languages.find(view_language) == admissible_translation_languages.end())
        view_language = "eng";

    try {
        std::vector<TranslationEntry> translations;

        LoadTranslations(db_connection, view_language, &translations);

        EmitHttpHeader();

        EmitHtmlPrefix();

        EmitDataset(translations);

        EmitJavascript();

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        std::cout << "Content-Type: text/plain\r\n\r\n"
                  << "Error: " << e.what() << '\n';

        return EXIT_FAILURE;
    }
}
