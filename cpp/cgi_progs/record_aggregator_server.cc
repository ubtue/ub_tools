/** \file    rss_webserver.cc
 *  \brief   Webserver for saving journal articles and delivering feeds
 *  \author  Hjordis Lindeboom (hjordis.lindeboom@uni-tuebingen.de)
 *
 *  \copyright 2025 Tübingen University Library.  All rights reserved.
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

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include "DbConnection.h"
#include "IniFile.h"
#include "SqlUtil.h"
#include "StringUtil.h"
#include "TimeUtil.h"
#include "UBTools.h"
#include "UrlUtil.h"
#include "XmlUtil.h"

const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "ub_tools.conf");

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct ArticleEntry {
    std::string article_link;
    std::string main_title;
    std::string journal_name;
    std::string pattern;
    std::string extraction_pattern;
    std::string crawl_pattern;
    std::string volume_pattern;
    std::string delivered_at;
};

std::map<std::string, std::string> parse_query_params(const std::string& target) {
    std::string scheme, username_password, authority, port, path, params, query, fragment, relative_url;
    std::map<std::string, std::string> query_map;

    if (not UrlUtil::ParseUrl(target, &scheme, &username_password, &authority, &port, &path, &params, &query, &fragment, &relative_url)) {
        query = "";
    }

    if (query.empty())
        return query_map;

    std::istringstream query_stream(query);
    std::string pair;
    while (std::getline(query_stream, pair, '&')) {
        auto eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = UrlUtil::UrlDecode(pair.substr(0, eq_pos));
            std::string value = UrlUtil::UrlDecode(pair.substr(eq_pos + 1));
            query_map[key] = value;
        }
    }

    return query_map;
}

std::vector<std::map<std::string, std::string>> parse_entries(const std::string& body) {
    std::istringstream stream(body);
    std::string line, key, value;
    std::vector<std::map<std::string, std::string>> entries;
    std::map<std::string, std::string> current_entry;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            if (not current_entry.empty()) {
                entries.push_back(current_entry);
                current_entry.clear();
            }
            continue;
        }

        std::vector<std::string> parts;
        StringUtil::Split(line, std::string("="), &parts);

        if (parts.size() >= 2) {
            key = parts[0];
            value = parts[1];
            current_entry[key] = value;
        }
    }

    if (not current_entry.empty())
        entries.push_back(current_entry);

    return entries;
}

std::optional<ArticleEntry> parse_entry(const std::map<std::string, std::string>& entry) {
    if (not entry.contains("article_link") || not entry.contains("journal")) {
        std::cerr << "Skipping entry due to missing article_link or journal.\n";
        return std::nullopt;
    }

    ArticleEntry article_entry;
    article_entry.article_link = entry.at("article_link");
    article_entry.main_title = entry.contains("main_title") ? entry.at("main_title") : article_entry.article_link;
    article_entry.journal_name = entry.at("journal");
    article_entry.pattern = entry.contains("pattern") ? entry.at("pattern") : "";
    article_entry.extraction_pattern = entry.contains("extraction_pattern") ? entry.at("extraction_pattern") : "";
    article_entry.crawl_pattern = entry.contains("crawl_pattern") ? entry.at("crawl_pattern") : "";
    article_entry.volume_pattern = entry.contains("volume_pattern") ? entry.at("volume_pattern") : "";

    article_entry.delivered_at = "NOW()";

    if (entry.contains("delivered_at")) {
        const std::string& ts_input = entry.at("delivered_at");
        try {
            time_t ts = static_cast<time_t>(std::stoll(ts_input));
            std::string datetime_str = SqlUtil::TimeTToDatetime(ts);

            if (SqlUtil::IsValidDatetime(datetime_str)) {
                article_entry.delivered_at = datetime_str;
            } else {
                std::cerr << "Converted datetime string is invalid: " << datetime_str << "\n";
            }
        } catch (...) {
            std::cerr << "Invalid delivered_at time_t format: " << ts_input << "\n";
        }
    }

    return article_entry;
}

bool parse_pagination(const std::map<std::string, std::string>& query_params, int& page_size, int& page_num) {
    page_size = 10;
    page_num = 1;

    try {
        if (query_params.contains("page_size"))
            page_size = std::stoi(query_params.at("page_size"));
        if (query_params.contains("page_num"))
            page_num = std::stoi(query_params.at("page_num"));
        if (page_size <= 0 || page_num <= 0)
            return false;
    } catch (...) {
        return false;
    }

    return true;
}

std::string get_path(const std::string& full_path) {
    auto pos = full_path.find('?');
    return pos != std::string::npos ? full_path.substr(0, pos) : full_path;
}

std::map<std::string, std::string> lookup_journal_info(DbConnection& db_connection, std::string journal_name) {
    std::map<std::string, std::string> journal_info;
    std::string query = "SELECT zeder_id, zeder_instance FROM retrokat_journals WHERE journal_name = "
                        + db_connection.escapeAndQuoteString(journal_name) + ";";
    DbResultSet result = db_connection.selectOrDie(query);

    if (not result.empty()) {
        DbRow row = result.getNextRow();
        journal_info["zeder_id"] = row.getValue("zeder_id", "");
        journal_info["zeder_instance"] = row.getValue("zeder_instance", "");
    }

    return journal_info;
}

bool insert_article(DbConnection& db_connection, const std::string& main_title, const std::string& article_link,
                    const std::string& zeder_id, const std::string& zeder_instance, const std::string& delivered_at,
                    const std::string& extraction_patterns) {
    std::string escaped_main_title = db_connection.escapeAndQuoteString(main_title);
    std::string escaped_article_link = db_connection.escapeAndQuoteString(article_link);
    std::string escaped_zeder_id = db_connection.escapeString(zeder_id, false);
    std::string escaped_zeder_instance = db_connection.escapeAndQuoteString(zeder_instance);
    std::string escaped_extraction_patterns = db_connection.escapeAndQuoteString(extraction_patterns);

    std::string delivered_at_sql;
    if (delivered_at == "NOW()") {
        delivered_at_sql = "NOW()";
    } else {
        delivered_at_sql = db_connection.escapeAndQuoteString(delivered_at);
    }

    std::string query =
        "INSERT INTO retrokat_articles (main_title, article_link, zeder_journal_id, zeder_instance, delivered_at, extraction_patterns) "
        "VALUES (" + escaped_main_title + ", " + escaped_article_link + ", " + escaped_zeder_id + ", " + escaped_zeder_instance + ", " + delivered_at_sql + ", " + escaped_extraction_patterns + ") "
        "ON DUPLICATE KEY UPDATE "
        "main_title = VALUES(main_title), "
        "delivered_at = VALUES(delivered_at), "
        "extraction_patterns = VALUES(extraction_patterns);";

    try {
        db_connection.queryOrDie(query);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "DB insert error: " << e.what() << "\n";
        return false;
    }
}

std::string build_extraction_json(const ArticleEntry& article) {
    nlohmann::json json_obj = { { "pattern", article.pattern },
                                { "regexes",
                                  { { "extraction_pattern", article.extraction_pattern },
                                    { "crawl_pattern", article.crawl_pattern },
                                    { "volume_pattern", article.volume_pattern } } } };
    return json_obj.dump(2);
}

int process_entries(DbConnection& db_connection, const std::vector<std::map<std::string, std::string>>& entries) {
    int inserted_count = 0;

    for (const auto& entry : entries) {
        auto article_entry = parse_entry(entry);
        if (not article_entry)
            continue;

        const ArticleEntry& article = *article_entry;

        std::string extraction_patterns = build_extraction_json(article);

        auto journal_info = lookup_journal_info(db_connection, article.journal_name);
        if (journal_info.empty()) {
            std::cerr << "Journal not found: " << article.journal_name << "\n";
            continue;
        }

        if (insert_article(db_connection, article.main_title, article.article_link, journal_info["zeder_id"],
                           journal_info["zeder_instance"], article.delivered_at, extraction_patterns))
        {
            ++inserted_count;
        }
    }

    return inserted_count;
}

std::string build_info_json(DbConnection& db_connection, const std::map<std::string, std::string>& journal_info, int page_size) {
    std::string query = "SELECT COUNT(*) AS total FROM retrokat_articles WHERE zeder_journal_id = "
                        + db_connection.escapeString(journal_info.at("zeder_id"), false)
                        + " AND zeder_instance = " + db_connection.escapeAndQuoteString(journal_info.at("zeder_instance")) + ";";

    DbResultSet result = db_connection.selectOrDie(query);
    int total_entries = 0;

    if (not result.empty()) {
        DbRow row = result.getNextRow();
        total_entries = std::stoi(row.getValue("total", "0"));
    }

    int total_pages = (total_entries + page_size - 1) / page_size;

    std::ostringstream json;
    json << "{ \"total_entries\": " << total_entries << ", \"page_size\": " << page_size << ", \"total_pages\": " << total_pages << " } \n";
    return json.str();
}

std::string build_feed(DbConnection& db_connection, const std::string& journal_name, const std::map<std::string, std::string>& journal_info,
                       int page_size, int page_num) {
    int offset = (page_num - 1) * page_size;
    std::ostringstream query;
    query << "SELECT main_title, article_link, delivered_at, extraction_patterns FROM retrokat_articles "
          << "WHERE zeder_journal_id = " << journal_info.at("zeder_id")
          << " AND zeder_instance = " << db_connection.escapeAndQuoteString(journal_info.at("zeder_instance")) << " LIMIT " << page_size
          << " OFFSET " << offset << ";";

    DbResultSet result = db_connection.selectOrDie(query.str());

    if (result.empty()) {
        return "No articles found.";
    }

    std::ostringstream feed;
    feed << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    feed << "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n";
    feed << "  <title>Feed for Journal " << journal_name << "</title>\n";
    feed << "  <id>http://localhost:9500/record_aggregator_server?journal=" << UrlUtil::UrlEncode(journal_name) << "</id>\n";
    feed << "  <updated>" << TimeUtil::GetCurrentDateAndTime(TimeUtil::ZULU_FORMAT, TimeUtil::UTC) << "</updated>\n";
    feed << "  <link rel=\"self\" type=\"application/atom+xml\" href=\"http://localhost:9500/record_aggregator_server?journal="
         << UrlUtil::UrlEncode(journal_name) << "\" />\n";

    for (DbRow row = result.getNextRow(); row; row = result.getNextRow()) {
        std::string link = row.getValue("article_link", "");
        std::string title = row.getValue("main_title", link);
        std::string updated = row.getValue("delivered_at", TimeUtil::GetCurrentDateAndTime(TimeUtil::ZULU_FORMAT, TimeUtil::UTC));
        std::string json = row.getValue("extraction_patterns", "");
        if (updated.find(' ') != std::string::npos) {
            std::replace(updated.begin(), updated.end(), ' ', 'T');
            updated += "Z";
        }

        feed << "  <entry>\n";
        feed << "    <title>" << XmlUtil::XmlEscape(title) << "</title>\n";
        feed << "    <link href=\"" << link << "\" />\n";
        feed << "    <id>" << link << "</id>\n";
        feed << "    <updated>" << updated << "</updated>\n";
        feed << "    <author><name>Feed Generator</name></author>\n";
        feed << "    <summary>Link to article: " << link << "</summary>\n";

        if (not json.empty()) {
            feed << "    <content type=\"html\">\n";
            feed << "      <![CDATA[\n";
            feed << "      <pre>" << json << "</pre>\n";
            feed << "      ]]>\n";
            feed << "    </content>\n";
        }

        feed << "  </entry>\n";
    }

    feed << "</feed>\n";
    return feed.str();
}

class session : public std::enable_shared_from_this<session> {
public:
    session(tcp::socket socket, std::string db_name, std::string db_user, std::string db_pass)
        : socket_(std::move(socket)), db_name_(std::move(db_name)), db_user_(std::move(db_user)), db_pass_(std::move(db_pass)) { }

    void start() { read_request(); }

protected:
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;

    std::string db_name_, db_user_, db_pass_;

    void read_request() {
        http::async_read(socket_, buffer_, req_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (not ec) {
                self->handle_request();
            } else {
                std::cerr << "Read error: " << ec.message() << "\n";
            }
        });
    }

    void handle_request() {
        DbConnection db_connection(DbConnection::MySQLFactory(db_name_, db_user_, db_pass_));
        if (db_connection.isNullConnection()) {
            send_response(http::status::internal_server_error, "Database connection failed");
            return;
        }

        if (req_.method() == http::verb::post && req_.target() == "/submit_feed") {
            handle_post_request(db_connection);
        } else if (req_.method() == http::verb::get) {
            handle_get_request(db_connection);
        } else {
            send_response(http::status::bad_request, "Unsupported request");
        }
    }

    void handle_post_request(DbConnection& db_connection) {
        auto entries = parse_entries(req_.body());
        int inserted_count = process_entries(db_connection, entries);
        send_response(http::status::ok, "Successfully processed " + std::to_string(inserted_count) + " entries.\n");
    }

    void handle_get_request(DbConnection& db_connection) {
        std::string host = req_.base()["Host"];
        std::string full_url = "http://" + host + std::string(req_.target());
        auto query_params = parse_query_params(full_url);
        std::string path = get_path(std::string(req_.target()));

        if (path != "/record_aggregator_server") {
            send_response(http::status::not_found, "Unknown endpoint.\n");
            return;
        }

        if (not query_params.contains("journal")) {
            send_response(http::status::bad_request, "Missing 'journal' parameter.\n");
            return;
        }

        auto journal_info = lookup_journal_info(db_connection, query_params["journal"]);
        if (journal_info.empty()) {
            send_response(http::status::not_found, "Journal not found.\n");
            return;
        }

        int page_size, page_num;
        if (not parse_pagination(query_params, page_size, page_num)) {
            send_response(http::status::bad_request, "Invalid page_size or page_num");
            return;
        }

        if (query_params.contains("info") && query_params["info"] == "1") {
            send_response(http::status::ok, build_info_json(db_connection, journal_info, page_size), "application/json");
            return;
        }

        std::string feed = build_feed(db_connection, query_params["journal"], journal_info, page_size, page_num);
        send_response(http::status::ok, feed, "application/atom+xml");
    }


    void send_response(http::status status, const std::string& content, const std::string& content_type = "text/plain") {
        auto res = std::make_shared<http::response<http::string_body>>(status, req_.version());
        res->set(http::field::server, "Boost.Beast");
        res->set(http::field::content_type, content_type);
        res->body() = content;
        res->prepare_payload();

        http::async_write(socket_, *res, [self = shared_from_this(), res](beast::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Write error: " << ec.message() << "\n";
                return;
            }

            beast::error_code shutdown_ec;
            self->socket_.shutdown(tcp::socket::shutdown_both, shutdown_ec);
        });
    }
};

class listener : public std::enable_shared_from_this<listener> {
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint, std::string db_name, std::string db_user, std::string db_pass)
        : acceptor_(ioc), db_name_(std::move(db_name)), db_user_(std::move(db_user)), db_pass_(std::move(db_pass)) {
        boost::system::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
    }

    void run() { accept(); }

protected:
    tcp::acceptor acceptor_;
    std::string db_name_, db_user_, db_pass_;

    void accept() {
        acceptor_.async_accept([self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            if (not ec) {
                std::cout << "Accepted connection from: " << socket.remote_endpoint() << "\n";
                std::make_shared<session>(std::move(socket), self->db_name_, self->db_user_, self->db_pass_)->start();
            } else {
                std::cerr << "Accept error: " << ec.message() << "\n";
            }
            self->accept();
        });
    }
};

int main() {
    try {
        net::io_context ioc;

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));

        auto endpoint = tcp::endpoint(tcp::v4(), 9500);
        std::make_shared<listener>(ioc, endpoint, sql_database, sql_username, sql_password)->run();

        std::cout << "Server running on http://localhost:9500\n";
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
    }

    return 0;
}
