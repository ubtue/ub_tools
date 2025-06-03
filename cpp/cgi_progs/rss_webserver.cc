#include <chrono>
#include <fstream>
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

struct Entry {
    std::string title;
    std::string link;
    std::string id;
};

std::string get_current_timestamp(int seconds_offset = 0) {
    auto now = std::chrono::system_clock::now() + std::chrono::seconds(seconds_offset);
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&now_c), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
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

std::string extract_single_entry_feed(const std::string& feed_xml, const std::string& which) {
    std::vector<std::string> entries;
    size_t pos = 0;
    while (true) {
        size_t start = feed_xml.find("<entry>", pos);
        if (start == std::string::npos)
            break;
        size_t end = feed_xml.find("</entry>", start);
        if (end == std::string::npos)
            break;
        end += 8;
        entries.push_back(feed_xml.substr(start, end - start));
        pos = end;
    }

    if (entries.empty())
        return feed_xml;

    std::string selected = (which == "last") ? entries.back() : entries.front();

    size_t header_end = feed_xml.find("<entry>");
    if (header_end == std::string::npos)
        header_end = feed_xml.find("</feed>");
    std::string header = feed_xml.substr(0, header_end);

    std::ostringstream out;
    out << header;
    out << selected << "\n";
    out << "</feed>\n";
    return out.str();
}

std::string paginate_feed(const std::string& feed_xml, int page_size, int page_num) {
    std::vector<std::string> entries;
    size_t pos = 0;
    while (true) {
        size_t start = feed_xml.find("<entry>", pos);
        if (start == std::string::npos)
            break;
        size_t end = feed_xml.find("</entry>", start);
        if (end == std::string::npos)
            break;

        end += 8;
        entries.push_back(feed_xml.substr(start, end - start));
        pos = end;
    }

    int total = entries.size();
    int from = std::max(0, (page_num - 1) * page_size);
    int to = std::min(from + page_size, total);

    size_t header_end = feed_xml.find("<entry>");
    if (header_end == std::string::npos)
        header_end = feed_xml.find("</feed>");
    std::string header = feed_xml.substr(0, header_end);

    std::ostringstream paginated;
    paginated << header;

    for (int i = from; i < to; ++i) {
        paginated << entries[i] << "\n";
    }

    paginated << "</feed>\n";
    return paginated.str();
}

void load_feeds(DbConnection& db_connection, std::map<std::string, std::string>& feeds) {
    std::string query = "SELECT feed_name, feed_content FROM retrokat_feeds;";
    DbResultSet result = db_connection.selectOrDie(query);

    for (DbRow row = result.getNextRow(); row; row = result.getNextRow()) {
        std::string feed_name = row.getValue("feed_name", "");
        std::string feed_content = row.getValue("feed_content", "");

        if (!feed_name.empty() && !feed_content.empty()) {
            feeds[feed_name] = feed_content;
        }
    }

    std::cout << "Loaded " << feeds.size() << " feeds.\n";
}

class session : public std::enable_shared_from_this<session> {
public:
    session(tcp::socket socket, std::map<std::string, std::string>& feeds, std::string db_name, std::string db_user, std::string db_pass)
        : socket_(std::move(socket)), feeds_(feeds), db_name_(std::move(db_name)), db_user_(std::move(db_user)),
          db_pass_(std::move(db_pass)) { }

    void start() { read_request(); }

protected:
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::map<std::string, std::string>& feeds_;

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
            std::istringstream stream(req_.body());
            std::string line, key, value, feed_name;
            std::vector<Entry> entries;
            Entry current_entry;

            while (std::getline(stream, line)) {
                if (line.empty()) {
                    if (!current_entry.link.empty()) {
                        current_entry.id = current_entry.link;
                        entries.push_back(current_entry);
                        current_entry = Entry();
                    }
                    continue;
                }

                auto delim_pos = line.find('=');
                if (delim_pos == std::string::npos)
                    continue;
                key = line.substr(0, delim_pos);
                value = line.substr(delim_pos + 1);

                if (key == "feed_name")
                    feed_name = value;
                else if (key == "title")
                    current_entry.title = value;
                else if (key == "link")
                    current_entry.link = value;
            }

            if (!current_entry.link.empty()) {
                current_entry.id = current_entry.link;
                entries.push_back(current_entry);
            }

            if (feed_name.empty()) {
                send_response(http::status::bad_request, "Missing feed_name");
                return;
            }

            if (feeds_.count(feed_name)) {
                append_to_feed(feed_name, entries, db_connection);
                send_response(http::status::ok, "Appended to existing feed: " + feed_name);
            } else {
                create_new_feed(feed_name, entries, db_connection);
                send_response(http::status::created, "Created new feed: " + feed_name);
            }
        } else if (req_.method() == http::verb::get) {
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

            std::string feed_name = query_params["journal"];
            std::string feed_data = feeds_.count(feed_name) ? feeds_[feed_name] : fetch_feed_from_db(feed_name, db_connection);


            if (feed_data.empty()) {
                send_response(http::status::not_found, "Feed not found");
                return;
            }

            if (query_params.count("info") && query_params["info"] == "1") {
                int page_size = query_params.count("page_size") ? std::stoi(query_params["page_size"]) : 10;

                std::vector<std::string> entries;
                size_t pos = 0;
                while (true) {
                    size_t start = feed_data.find("<entry>", pos);
                    if (start == std::string::npos)
                        break;
                    size_t end = feed_data.find("</entry>", start);
                    if (end == std::string::npos)
                        break;

                    end += 8;
                    entries.push_back(feed_data.substr(start, end - start));
                    pos = end;
                }

                int total_entries = entries.size();
                int total_pages = (total_entries + page_size - 1) / page_size;

                std::ostringstream json;
                json << "{ \"total_entries\": " << total_entries << ", \"page_size\": " << page_size << ", \"total_pages\": " << total_pages
                     << " }";

                send_response(http::status::ok, json.str());
                return;
            }

            if (query_params.count("entry")) {
                std::string which = query_params["entry"];
                if (which == "first" || which == "last") {
                    std::string single_entry_feed = extract_single_entry_feed(feed_data, which);
                    send_response(http::status::ok, single_entry_feed);
                    return;
                }
            }

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

            std::string paginated_feed = paginate_feed(feed_data, page_size, page_num);
            send_response(http::status::ok, paginated_feed);
        } else {
            send_response(http::status::bad_request, "Unsupported request");
        }
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

    void insert_feed_into_db(const std::string& feed_name, const std::string& content, DbConnection& db_connection) {
        std::string safe_feed_name = escape_sql(feed_name);
        std::string safe_content = escape_sql(content);

        std::string query =
            "INSERT INTO retrokat_feeds (feed_name, feed_content) VALUES ('" + safe_feed_name + "', '" + safe_content + "') "
            "ON DUPLICATE KEY UPDATE feed_content = VALUES(feed_content);";

        db_connection.queryOrDie(query);
    }

    std::string fetch_feed_from_db(const std::string& feed_name, DbConnection& db_connection) {
        std::string query = "SELECT feed_content FROM retrokat_feeds WHERE feed_name = '" + feed_name + "';";
        DbResultSet result = db_connection.selectOrDie(query);

        if (!result.empty()) {
            DbRow row = result.getNextRow();

            try {
                return row["feed_content"];
            } catch (const std::out_of_range&) {
                std::cerr << "Column 'feed_content' not found!" << std::endl;
            }
        }

        return "";
    }

    void create_new_feed(const std::string& feed_name, const std::vector<Entry>& entries, DbConnection& db_connection) {
        std::ostringstream rss;
        rss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        rss << "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n";
        rss << "  <title>Generated Atom Feed</title>\n";
        rss << "  <id>http://localhost:8080/" << feed_name << "</id>\n";
        rss << "  <updated>" << get_current_timestamp() << "</updated>\n";
        rss << "  <link href=\"http://localhost:8080/" << feed_name << "\" />\n";

        for (const auto& entry : entries) {
            rss << "  <entry>\n";
            rss << "    <title>" << entry.title << "</title>\n";
            rss << "    <link href=\"" << entry.link << "\" />\n";
            rss << "    <id>" << entry.link << "</id>\n";
            rss << "    <updated>" << get_current_timestamp() << "</updated>\n";
            rss << "    <author><name>Feed Generator</name></author>\n";
            rss << "    <summary>Link to article: " << entry.link << "</summary>\n";
            rss << "  </entry>\n";
        }

        rss << "</feed>\n";

        std::string content = rss.str();
        feeds_[feed_name] = content;
        insert_feed_into_db(feed_name, content, db_connection);
    }

    void append_to_feed(const std::string& feed_name, const std::vector<Entry>& new_entries, DbConnection& db_connection) {
        std::string& existing_feed = feeds_[feed_name];

        std::set<std::string> existing_ids;
        std::istringstream iss(existing_feed);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("<id>") != std::string::npos) {
                size_t start = line.find("<id>") + 4;
                size_t end = line.find("</id>");
                if (start != std::string::npos && end != std::string::npos)
                    existing_ids.insert(line.substr(start, end - start));
            }
        }

        std::ostringstream new_feed;
        size_t pos = existing_feed.rfind("</feed>");
        if (pos == std::string::npos)
            return;

        new_feed << existing_feed.substr(0, pos);

        for (const auto& entry : new_entries) {
            if (existing_ids.count(entry.link)) {
                std::cout << "Skipping duplicate entry with link: " << entry.link << "\n";
                continue;
            }

            new_feed << "  <entry>\n";
            new_feed << "    <title>" << entry.title << "</title>\n";
            new_feed << "    <link href=\"" << entry.link << "\" />\n";
            new_feed << "    <id>" << entry.link << "</id>\n";
            new_feed << "    <updated>" << get_current_timestamp() << "</updated>\n";
            new_feed << "    <author><name>Feed Generator</name></author>\n";
            new_feed << "    <summary>Link to article: " << entry.link << "</summary>\n";
            new_feed << "  </entry>\n";
        }

        new_feed << "</feed>\n";

        feeds_[feed_name] = new_feed.str();
        insert_feed_into_db(feed_name, feeds_[feed_name], db_connection);
    }
};

class listener : public std::enable_shared_from_this<listener> {
public:
    listener(net::io_context& ioc, tcp::endpoint endpoint, std::map<std::string, std::string>& feeds, std::string db_name,
             std::string db_user, std::string db_pass)
        : acceptor_(ioc), feeds_(feeds), db_name_(std::move(db_name)), db_user_(std::move(db_user)), db_pass_(std::move(db_pass)) {
        boost::system::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
    }

    void run() { accept(); }

protected:
    tcp::acceptor acceptor_;
    std::map<std::string, std::string>& feeds_;
    std::string db_name_, db_user_, db_pass_;

    void accept() {
        acceptor_.async_accept([self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "Accepted connection from: " << socket.remote_endpoint() << "\n";
                std::make_shared<session>(std::move(socket), self->feeds_, self->db_name_, self->db_user_, self->db_pass_)->start();
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
        std::map<std::string, std::string> feeds;

        const IniFile ini_file(CONF_FILE_PATH);
        const std::string sql_database(ini_file.getString("Database", "sql_database"));
        const std::string sql_username(ini_file.getString("Database", "sql_username"));
        const std::string sql_password(ini_file.getString("Database", "sql_password"));
        {
            DbConnection db_connection(DbConnection::MySQLFactory(sql_database, sql_username, sql_password));
            if (db_connection.isNullConnection()) {
                std::cerr << "Failed to connect to the MySQL database.\n";
                return 1;
            }

            std::cout << "Loading existing feeds from the database...\n";
            load_feeds(db_connection, feeds);
        }

        auto endpoint = tcp::endpoint(tcp::v4(), 8080);
        std::make_shared<listener>(ioc, endpoint, feeds, sql_database, sql_username, sql_password)->run();

        std::cout << "Server running on http://localhost:8080\n";
        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
    }

    return 0;
}
