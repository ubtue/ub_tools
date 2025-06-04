#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/filesystem.hpp>
#include "DbConnection.h"
#include "IniFile.h"
#include "UBTools.h"

const std::string CONF_FILE_PATH(UBTools::GetTuelibPath() + "ub_tools.conf");

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace fs = boost::filesystem;
using tcp = net::ip::tcp;

std::string get_current_timestamp(int seconds_offset = 0) {
    auto now = std::chrono::system_clock::now() + std::chrono::seconds(seconds_offset);
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* gmt = std::gmtime(&now_c);

    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", gmt);
    return std::string(buffer);
}

bool is_valid_mysql_datetime(const std::string& dt) {
    if (dt.size() != 19)
        return false;
    std::tm t = {};
    std::istringstream ss(dt);
    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    return !ss.fail();
}

std::string escape_sql(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        if (c == '\'')
            escaped += "\\'";
        else if (c == '\\')
            escaped += "\\\\";
        else
            escaped += c;
    }
    return escaped;
}

std::map<std::string, std::string> parse_query_params(const std::string& target) {
    std::map<std::string, std::string> params;
    auto pos = target.find('?');
    if (pos == std::string::npos)
        return params;

    std::string query = target.substr(pos + 1);
    std::istringstream query_stream(query);
    std::string pair;

    while (std::getline(query_stream, pair, '&')) {
        auto eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            params[key] = value;
        }
    }

    return params;
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
            if (!ec) {
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
        std::istringstream stream(req_.body());
        std::string line, key, value;

        std::vector<std::map<std::string, std::string>> entries;
        std::map<std::string, std::string> current_entry;

        while (std::getline(stream, line)) {
            if (line.empty()) {
                if (!current_entry.empty()) {
                    entries.push_back(current_entry);
                    current_entry.clear();
                }
                continue;
            }

            auto delim_pos = line.find('=');
            if (delim_pos == std::string::npos)
                continue;

            key = line.substr(0, delim_pos);
            value = line.substr(delim_pos + 1);

            current_entry[key] = value;
        }

        if (!current_entry.empty())
            entries.push_back(current_entry);

        int inserted_count = 0;
        for (const auto& entry : entries) {
            if (!entry.count("article_link") || !entry.count("zeder_journal_id") || !entry.count("zeder_instance")) {
                std::cerr << "Skipping entry due to missing fields.\n";
                continue;
            }

            std::string main_title = escape_sql(entry.at("main_title"));
            std::string article_link = escape_sql(entry.at("article_link"));
            std::string journal_id = escape_sql(entry.at("zeder_journal_id"));
            std::string instance = escape_sql(entry.at("zeder_instance"));
            std::string delivered_at;
            if (entry.count("delivered_at")) {
                std::string raw = entry.at("delivered_at");
                std::replace(raw.begin(), raw.end(), 'T', ' ');
                if (!raw.empty() && raw.back() == 'Z')
                    raw.pop_back();

                if (!is_valid_mysql_datetime(raw)) {
                    std::cerr << "Invalid datetime format: " << raw << ". Skipping entry.\n";
                    continue;
                }
                delivered_at = "'" + escape_sql(raw) + "'";
            } else {
                delivered_at = "NOW()";
            }


            std::string insert_query =
                "INSERT INTO retrokat_articles (main_title, article_link, zeder_journal_id, zeder_instance, delivered_at) "
                "VALUES ('" + main_title + "', '" + article_link + "', " + journal_id + ", '" + instance + "', " + delivered_at + ") "
                "ON DUPLICATE KEY UPDATE main_title = VALUES(main_title), delivered_at = VALUES(delivered_at);";

            try {
                db_connection.queryOrDie(insert_query);
                ++inserted_count;
            } catch (const std::exception& e) {
                std::cerr << "Insert failed for article_link=" << article_link << ": " << e.what() << "\n";
            }
        }

        std::ostringstream res;
        res << "Successfully processed " << inserted_count << " entries.";
        send_response(http::status::ok, res.str());
    }

    void handle_get_request(DbConnection& db_connection) {
        std::string full_path = req_.target().to_string();
        auto query_params = parse_query_params(full_path);

        std::string path = full_path.substr(0, full_path.find('?'));
        if (path != "/retrokat_webserver") {
            send_response(http::status::not_found, "Unknown endpoint");
            return;
        }

        if (!query_params.count("journal")) {
            send_response(http::status::bad_request, "Missing 'journal' parameter");
            return;
        }

        int journal_id = std::stoi(query_params["journal"]);

        int page_size = 10;
        int page_num = 1;

        try {
            if (query_params.count("page_size"))
                page_size = std::stoi(query_params["page_size"]);
            if (query_params.count("page_num"))
                page_num = std::stoi(query_params["page_num"]);

            if (page_size <= 0 || page_num <= 0)
                throw std::invalid_argument("Non-positive values");
        } catch (...) {
            send_response(http::status::bad_request, "Invalid page_size or page_num");
            return;
        }

        if (query_params.count("info") && query_params["info"] == "1") {
            std::string count_query =
                "SELECT COUNT(*) AS total FROM retrokat_articles WHERE zeder_journal_id = " + std::to_string(journal_id) + ";";
            DbResultSet count_result = db_connection.selectOrDie(count_query);
            int total_entries = 0;

            if (!count_result.empty()) {
                DbRow row = count_result.getNextRow();
                total_entries = std::stoi(row.getValue("total", "0"));
            }

            int total_pages = (total_entries + page_size - 1) / page_size;

            std::ostringstream json;
            json << "{ \"total_entries\": " << total_entries << ", \"page_size\": " << page_size << ", \"total_pages\": " << total_pages
                 << " }\n";

            send_response(http::status::ok, json.str());
            return;
        }

        int offset = (page_num - 1) * page_size;

        std::ostringstream query;
        query << "SELECT main_title, article_link, delivered_at FROM retrokat_articles "
              << "WHERE zeder_journal_id = " << journal_id << " "
              << "LIMIT " << page_size << " OFFSET " << offset << ";";

        DbResultSet result = db_connection.selectOrDie(query.str());

        if (result.empty()) {
            send_response(http::status::not_found, "No articles found for given journal_id");
            return;
        }

        std::ostringstream feed;
        feed << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        feed << "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n";
        feed << "  <title>Feed for Journal ID " << journal_id << "</title>\n";
        feed << "  <id>http://localhost:8080/retrokat_webserver?journal=" << journal_id << "</id>\n";
        feed << "  <updated>" << get_current_timestamp() << "</updated>\n";
        feed << "  <link href=\"http://localhost:8080/retrokat_webserver?journal=" << journal_id << "\" />\n";

        for (DbRow row = result.getNextRow(); row; row = result.getNextRow()) {
            std::string link = row.getValue("article_link", "");
            std::string title = row.getValue("main_title", link);
            std::string updated = row.getValue("delivered_at", get_current_timestamp());
            if (updated.find(' ') != std::string::npos) {
                std::replace(updated.begin(), updated.end(), ' ', 'T');
                updated += "Z";
            }

            feed << "  <entry>\n";
            feed << "    <title>" << title << "</title>\n";
            feed << "    <link href=\"" << link << "\" />\n";
            feed << "    <id>" << link << "</id>\n";
            feed << "    <updated>" << updated << "</updated>\n";
            feed << "    <author><name>Feed Generator</name></author>\n";
            feed << "    <summary>Link to article: " << link << "</summary>\n";
            feed << "  </entry>\n";
        }

        feed << "</feed>\n";

        send_response(http::status::ok, feed.str());
    }


    void send_response(http::status status, const std::string& content) {
        auto res = std::make_shared<http::response<http::string_body>>(status, req_.version());
        res->set(http::field::server, "Boost.Beast");
        res->set(http::field::content_type, "text/plain");
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
            if (!ec) {
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

        auto endpoint = tcp::endpoint(tcp::v4(), 8080);
        std::make_shared<listener>(ioc, endpoint, sql_database, sql_username, sql_password)->run();

        std::cout << "Server running on http://localhost:8080\n";
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
    }

    return 0;
}
