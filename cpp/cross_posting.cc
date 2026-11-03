/**
 * \brief Cross-posting client for Centralizing Posts Service API (ex. Mixpost).
 * \author Steven Lolong (steven.lolong@uni-tuebingen.de)
 * \copyright 2026 Universitätsbibliothek Tübingen.  All rights reserved.
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


#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include "DbConnection.h"
#include "Downloader.h"
#include "HtmlUtil.h"
#include "IniFile.h"
#include "JSON.h"
#include "REST.h"
#include "Url.h"


// Table structure for cross_posting:
// CREATE DATABASE IF NOT EXISTS cross_posting
//   CHARACTER SET utf8mb4
//   COLLATE utf8mb4_unicode_ci;

// CREATE TABLE IF NOT EXISTS cross_posting.posts (
//     workspace_uuid CHAR(36) NOT NULL,
//     post_uuid VARCHAR(64) NOT NULL,
//     status VARCHAR(64) NOT NULL DEFAULT '',
//     created_at VARCHAR(64) NOT NULL DEFAULT '',
//     content LONGTEXT,
//     raw_json LONGTEXT NOT NULL,
//     synced_at TIMESTAMP NOT NULL
//         DEFAULT CURRENT_TIMESTAMP
//         ON UPDATE CURRENT_TIMESTAMP,

//     PRIMARY KEY (workspace_uuid, post_uuid)
// ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


// file: cross_posting.conf
// [Mixpost]
// api_base_url = http://127.0.0.1:8765/api
// api_token = local-test-token
// workspace_uuid = 11111111-1111-4111-8111-111111111111
// username = mock_newsroom

// [Database]
// sql_database = cross_posting
// sql_username = cross_posting_user
// sql_password = YOUR_MYSQL_PASSWORD
// sql_host = localhost
// sql_port = 3306

namespace {

std::string GetConfigPath() {
    // Default system-wide configuration for ub_tools.
    return "/usr/local/var/lib/tuelib/cross_posting.conf";
}

struct CommandLineOptions {
    std::string command;
    std::string config_path;
    std::vector<std::string> arguments;
    bool send = false;
};

CommandLineOptions ParseArguments(int argc, char *argv[]) {
    CommandLineOptions options;
    options.config_path = GetConfigPath();

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);

        if (arg == "--config") {
            if (i + 1 >= argc)
                throw std::runtime_error("Missing path after --config");

            options.config_path = argv[++i];

            if (options.config_path.empty() || options.config_path[0] == '-')
                throw std::runtime_error("Invalid configuration path");

        } else if (arg == "--send") {
            if (options.send)
                throw std::runtime_error("Duplicate --send option");

            options.send = true;

        } else if (options.command.empty()) {
            options.command = arg;

        } else {
            options.arguments.push_back(arg);
        }
    }

    return options;
}

bool IsValidUuid(const std::string &uuid) {
    if (uuid.size() != 36)
        return false;

    for (std::size_t i = 0; i < uuid.size(); ++i) {
        const bool separator = i == 8 || i == 13 || i == 18 || i == 23;

        if (separator) {
            if (uuid[i] != '-')
                return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(uuid[i]))) {
            return false;
        }
    }

    return true;
}


std::string ExtractOriginalContent(const JSON::ObjectNode &post) {
    const auto versions = post.getOptionalArrayNode("versions");

    if (!versions)
        return "";

    for (const auto &entry : *versions) {
        if (!entry || entry->getType() != JSON::JSONNode::OBJECT_NODE)
            continue;

        const auto version = std::static_pointer_cast<JSON::ObjectNode>(entry);

        if (!version->getOptionalBooleanValue("is_original", false))
            continue;

        const auto contents = version->getOptionalArrayNode("content");

        if (!contents)
            return "";

        std::string body;

        for (const auto &item : *contents) {
            if (!item || item->getType() != JSON::JSONNode::OBJECT_NODE)
                continue;

            const auto content = std::static_pointer_cast<JSON::ObjectNode>(item);

            const std::string text = content->getOptionalStringValue("body");

            if (!text.empty()) {
                if (!body.empty())
                    body += "\n";

                body += text;
            }
        }

        return HtmlUtil::StripHtmlTags(body);
    }

    return "";
}


class CrossPostingClient {
private:
    std::string base_url_;
    std::string token_;

public:
    CrossPostingClient(std::string base_url, std::string token): base_url_(std::move(base_url)), token_(std::move(token)) {
        while (!base_url_.empty() && base_url_.back() == '/') {
            base_url_.pop_back();
        }

        const bool https = base_url_.rfind("https://", 0) == 0;

        const bool localhost = base_url_.rfind("http://localhost:", 0) == 0 || base_url_.rfind("http://127.0.0.1:", 0) == 0;

        if (!https && !localhost)
            throw std::runtime_error("API must use HTTPS or local HTTP");

        if (base_url_.find_first_of("?#@\r\n") != std::string::npos)
            throw std::runtime_error("Invalid API base URL");

        if (token_.empty() || token_.find_first_of("\r\n") != std::string::npos)
            throw std::runtime_error("Invalid API token");
    }

    // Validate a workspace-relative API path. Only GET may include a query string.
    static void ValidateApiPath(const std::string &path, const bool allow_query) {
        if (path.empty() || path.front() != '/' || path.rfind("//", 0) == 0 || path.find("..") != std::string::npos
            || path.find_first_of(allow_query ? "\r\n#" : "\r\n?#") != std::string::npos)
        {
            throw std::runtime_error("Invalid API path");
        }
    }

    Downloader::Params MakeRequestParams(const bool has_json_body) const {
        Downloader::Params params;
        params.additional_headers_ = { "Accept: application/json", "Authorization: Bearer " + token_ };

        if (has_json_body)
            params.additional_headers_.push_back("Content-Type: application/json");

        // QueryResponse returns HTTP errors with status, headers and body.
        // Never silently follow redirects with a bearer token.
        params.fail_on_error_ = false;
        params.follow_redirects_ = false;
        params.ignore_ssl_certificates_ = false;
        params.use_cookies_txt_ = false;
        return params;
    }

    static std::string SanitizeServerText(std::string text, const std::size_t limit) {
        if (text.size() > limit)
            text.resize(limit);

        for (char &ch : text) {
            if (static_cast<unsigned char>(ch) < 32 || ch == 127)
                ch = ' ';
        }
        return text;
    }

    static std::string HttpErrorMessage(const REST::Response &response) {
        std::string detail;

        if (!response.body.empty()) {
            try {
                JSON::Parser parser(response.body);
                std::shared_ptr<JSON::JSONNode> error_json;

                if (parser.parse(&error_json) && error_json && error_json->getType() == JSON::JSONNode::OBJECT_NODE) {
                    const auto object = std::static_pointer_cast<JSON::ObjectNode>(error_json);
                    detail = object->getOptionalStringValue("message");
                    if (detail.empty())
                        detail = object->getOptionalStringValue("error");

                    if (response.status_code == 422) {
                        const auto errors = object->getOptionalObjectNode("errors");
                        if (errors) {
                            if (!detail.empty())
                                detail += " ";
                            detail += errors->toString();
                        }
                    }
                }
            } catch (const std::exception &) {
                // Keep the HTTP status if the error payload is malformed.
            }
        }

        std::string error = "Mixpost API returned HTTP " + std::to_string(response.status_code);
        detail = SanitizeServerText(std::move(detail), 500);
        if (!detail.empty())
            error += ": " + detail;

        if (response.status_code == 429 && response.headers.isValid()) {
            const std::string retry_after = SanitizeServerText(response.headers.getRetryAfter(), 64);
            if (!retry_after.empty())
                error += " (Retry-After: " + retry_after + ")";
        }

        return error;
    }

    static std::shared_ptr<JSON::JSONNode> ReadJsonResponse(const REST::Response &response, const bool allow_no_content) {
        if (!response.isSuccess())
            throw std::runtime_error(HttpErrorMessage(response));

        if (response.status_code == 204) {
            if (allow_no_content)
                return nullptr;
            throw std::runtime_error("Unexpected HTTP 204 from Mixpost");
        }

        if (response.body.empty())
            throw std::runtime_error("Mixpost returned an empty JSON response (HTTP " + std::to_string(response.status_code) + ")");

        JSON::Parser parser(response.body);
        std::shared_ptr<JSON::JSONNode> result;
        if (!parser.parse(&result) || !result)
            throw std::runtime_error("Mixpost returned invalid JSON (HTTP " + std::to_string(response.status_code) + ")");

        return result;
    }

    std::shared_ptr<JSON::JSONNode> Get(const std::string &path) const {
        ValidateApiPath(path, /* allow_query = */ true);

        const REST::Response response = REST::QueryResponse(Url(base_url_ + path), REST::GET, "", MakeRequestParams(false));
        return ReadJsonResponse(response, /* allow_no_content = */ false);
    }

    std::shared_ptr<JSON::JSONNode> PostJson(const std::string &path, const JSON::JSONNode &payload) const {
        ValidateApiPath(path, /* allow_query = */ false);

        const REST::Response response = REST::QueryResponse(Url(base_url_ + path), REST::POST, payload.toString(), MakeRequestParams(true));
        return ReadJsonResponse(response, /* allow_no_content = */ true);
    }


    void Ping() const {
        const auto response = Get("/ping");

        if (!response || response->getType() != JSON::JSONNode::OBJECT_NODE)
            throw std::runtime_error("Invalid ping response");

        const auto object = std::static_pointer_cast<JSON::ObjectNode>(response);

        if (object->getOptionalStringValue("status") != "ok")
            throw std::runtime_error("Ping failed: status is not ok");

        std::cout << "CrossPosting: connection OK\n";
    }


    void GetPosts(const std::string &workspace) const {
        if (!IsValidUuid(workspace)) {
            throw std::runtime_error("Invalid workspace UUID");
        }

        constexpr unsigned MAX_PAGES = 1000;

        std::unordered_set<std::string> seen_uuids;

        std::size_t total_posts = 0;
        std::size_t duplicates = 0;
        unsigned pages_processed = 0;
        bool completed = false;

        for (unsigned page = 1; page <= MAX_PAGES; ++page) {
            const std::string path = "/" + workspace + "/posts?page=" + std::to_string(page);

            // GET via REST::QueryResponse(), including HTTP status validation.
            const auto response = Get(path);

            if (!response || response->getType() != JSON::JSONNode::OBJECT_NODE) {
                throw std::runtime_error("Invalid posts JSON response");
            }

            const auto root = std::static_pointer_cast<JSON::ObjectNode>(response);

            const auto posts = root->getOptionalArrayNode("data");

            const auto meta = root->getOptionalObjectNode("meta");

            if (!posts || !meta) {
                throw std::runtime_error("Missing data or meta in posts response");
            }

            const std::int64_t current_page = meta->getOptionalIntegerValue("current_page", 0);

            const std::int64_t last_page = meta->getOptionalIntegerValue("last_page", 0);

            if (current_page != page || last_page < current_page) {
                throw std::runtime_error("Invalid pagination metadata");
            }

            ++pages_processed;

            std::cout << "\n=== Page " << current_page << " / " << last_page << " ===\n"
                      << "Items: " << posts->size() << "\n\n";

            for (const auto &entry : *posts) {
                if (!entry || entry->getType() != JSON::JSONNode::OBJECT_NODE) {
                    throw std::runtime_error("Invalid post object");
                }

                const auto post = std::static_pointer_cast<JSON::ObjectNode>(entry);

                const std::string uuid = post->getOptionalStringValue("uuid");

                if (uuid.empty()) {
                    throw std::runtime_error("Post has no UUID");
                }

                // Avoid duplicates between pages.
                if (!seen_uuids.insert(uuid).second) {
                    ++duplicates;
                    continue;
                }

                const std::string status = post->getOptionalStringValue("status");

                const std::string created = post->getOptionalStringValue("created_at");

                const std::string published = post->getOptionalStringValue("published_at");

                std::string body;

                const auto versions = post->getOptionalArrayNode("versions");

                if (versions) {
                    for (const auto &version_entry : *versions) {
                        if (!version_entry || version_entry->getType() != JSON::JSONNode::OBJECT_NODE) {
                            continue;
                        }

                        const auto version = std::static_pointer_cast<JSON::ObjectNode>(version_entry);

                        if (!version->getOptionalBooleanValue("is_original", false)) {
                            continue;
                        }

                        const auto content = version->getOptionalArrayNode("content");

                        if (content) {
                            for (const auto &item : *content) {
                                if (!item || item->getType() != JSON::JSONNode::OBJECT_NODE) {
                                    continue;
                                }

                                const auto content_object = std::static_pointer_cast<JSON::ObjectNode>(item);

                                const std::string part = content_object->getOptionalStringValue("body");

                                if (!part.empty()) {
                                    if (!body.empty())
                                        body += "\n";

                                    body += part;
                                }
                            }
                        }

                        break;
                    }
                }

                body = HtmlUtil::StripHtmlTags(body);

                ++total_posts;

                std::cout << "UUID      : " << uuid << '\n'
                          << "Status    : " << status << '\n'
                          << "Created   : " << created << '\n'
                          << "Published : " << published << '\n'
                          << "Content   : " << body << "\n\n";
            }

            // Stop only after the last confirmed page.
            if (current_page >= last_page) {
                completed = true;
                break;
            }
        }

        if (!completed) {
            throw std::runtime_error("Pagination exceeded safety limit");
        }

        std::cout << "=== Harvest Summary ===\n"
                  << "Pages processed : " << pages_processed << '\n'
                  << "Unique posts    : " << total_posts << '\n'
                  << "Duplicates      : " << duplicates << '\n'
                  << "Status          : SUCCESS\n";
    }


    void SyncPosts(const std::string &workspace, const IniFile &config) const {
        if (!IsValidUuid(workspace))
            throw std::runtime_error("Invalid workspace UUID");

        // MySQLFactory reads sql_database, sql_username, sql_password,
        // sql_host, and sql_port from the [Database] section.
        const std::string database = config.getString("Database", "sql_database");
        auto db = DbConnection::MySQLFactory(config, "Database", DbConnection::TZ_UTC);

        if (db.isNullConnection())
            throw std::runtime_error("Cannot connect to MySQL");

        constexpr unsigned MAX_PAGES = 1000;

        std::unordered_set<std::string> seen;

        std::size_t total_synced = 0;
        std::size_t duplicates = 0;
        unsigned pages_synced = 0;
        bool completed = false;

        std::cout << "=== CrossPosting Synchronization ===\n"
                  << "Workspace: " << workspace << '\n'
                  << "Database : " << database << "\n\n";

        for (unsigned page = 1; page <= MAX_PAGES; ++page) {
            const auto response = Get("/" + workspace + "/posts?page=" + std::to_string(page));

            if (!response || response->getType() != JSON::JSONNode::OBJECT_NODE)
                throw std::runtime_error("Invalid posts JSON response");

            const auto root = std::static_pointer_cast<JSON::ObjectNode>(response);

            const auto posts = root->getOptionalArrayNode("data");

            const auto meta = root->getOptionalObjectNode("meta");

            if (!posts || !meta)
                throw std::runtime_error("Missing posts data or pagination metadata");

            const std::int64_t current_page = meta->getOptionalIntegerValue("current_page", 0);

            const std::int64_t last_page = meta->getOptionalIntegerValue("last_page", 0);

            if (current_page != page || last_page < current_page || last_page > MAX_PAGES)
                throw std::runtime_error("Invalid pagination metadata");

            // All writes of one page are atomic.
            {
                DbTransaction transaction(&db);

                for (const auto &entry : *posts) {
                    if (!entry || entry->getType() != JSON::JSONNode::OBJECT_NODE)
                        throw std::runtime_error("Invalid post object");

                    const auto post = std::static_pointer_cast<JSON::ObjectNode>(entry);

                    const std::string uuid = post->getOptionalStringValue("uuid");

                    if (uuid.empty() || uuid.size() > 64)
                        throw std::runtime_error("Missing or invalid post UUID");

                    if (!seen.insert(uuid).second) {
                        ++duplicates;
                        continue;
                    }

                    const std::string status = post->getOptionalStringValue("status");

                    const std::string created_at = post->getOptionalStringValue("created_at");

                    const std::string content = ExtractOriginalContent(*post);

                    const std::string raw_json = post->toString();

                    const std::string sql =
                    "INSERT INTO posts "
                    "(workspace_uuid, post_uuid, "
                    "status, created_at, content, raw_json) "
                    "VALUES (" +
                    db.escapeAndQuoteString(workspace) + "," +
                    db.escapeAndQuoteString(uuid) + "," +
                    db.escapeAndQuoteString(status) + "," +
                    db.escapeAndQuoteString(created_at) + "," +
                    db.escapeAndQuoteString(content) + "," +
                    db.escapeAndQuoteString(raw_json) +
                    ") ON DUPLICATE KEY UPDATE "
                    "status = VALUES(status), "
                    "created_at = VALUES(created_at), "
                    "content = VALUES(content), "
                    "raw_json = VALUES(raw_json)";

                    if (!db.query(sql))
                        throw std::runtime_error("MySQL write failed: " + db.getLastErrorMessage());

                    ++total_synced;

                    std::cout << "Synced: " << uuid << '\n';
                }

                transaction.commit();
            }

            ++pages_synced;

            std::cout << "Page " << current_page << "/" << last_page << " committed.\n";

            if (current_page >= last_page) {
                completed = true;
                break;
            }
        }

        if (!completed)
            throw std::runtime_error("Synchronization exceeded page limit");

        std::cout << "\n=== Synchronization Summary ===\n"
                  << "Pages       : " << pages_synced << '\n'
                  << "Posts synced: " << total_synced << '\n'
                  << "Duplicates  : " << duplicates << '\n'
                  << "Status      : SUCCESS\n";
    }


    void CreateDraft(const std::string &workspace, const std::int64_t account_id, const std::string &message, const bool send) const {
        if (!IsValidUuid(workspace))
            throw std::runtime_error("Invalid workspace UUID");

        if (account_id <= 0)
            throw std::runtime_error("Account ID must be positive");

        if (message.empty())
            throw std::runtime_error("Post content cannot be empty");

        // Build the target account list.
        auto accounts = std::make_shared<JSON::ArrayNode>();

        accounts->push_back(std::make_shared<JSON::IntegerNode>(account_id));

        // Build the content object.
        auto content_item = std::make_shared<JSON::ObjectNode>();

        content_item->insert("body", std::make_shared<JSON::StringNode>(message));

        content_item->insert("media", std::make_shared<JSON::ArrayNode>());

        auto content = std::make_shared<JSON::ArrayNode>();

        content->push_back(content_item);

        // Build the original content version.
        auto version = std::make_shared<JSON::ObjectNode>();

        version->insert("account_id", std::make_shared<JSON::IntegerNode>(0));

        version->insert("is_original", std::make_shared<JSON::BooleanNode>(true));

        version->insert("content", content);

        auto versions = std::make_shared<JSON::ArrayNode>();

        versions->push_back(version);

        // Assemble the Mixpost request.
        JSON::ObjectNode request;

        request.insert("accounts", accounts);
        request.insert("versions", versions);

        request.insert("schedule", std::make_shared<JSON::BooleanNode>(false));

        request.insert("schedule_now", std::make_shared<JSON::BooleanNode>(false));

        request.insert("queue", std::make_shared<JSON::BooleanNode>(false));

        std::cout << "=== CrossPosting Draft ===\n"
                  << "Workspace  : " << workspace << '\n'
                  << "Account ID : " << account_id << '\n'
                  << "Content    : " << message << '\n'
                  << "Payload    : " << request.toString() << "\n\n";

        if (!send) {
            std::cout << "DRY RUN: Request not sent.\n"
                      << "Add --send to create the draft.\n";
            return;
        }

        const std::string path = "/" + workspace + "/posts";

        const auto response = PostJson(path, request);

        // HTTP 204 No Content is a successful draft request.
        if (!response) {
            std::cout << "Draft request completed successfully (HTTP 204 No Content).\n";
            return;
        }

        std::cout << "Draft creation request completed.\n"
                  << "Response: " << response->toString() << '\n';
    }


    void ListAccounts(const std::string &workspace) const {
        if (!IsValidUuid(workspace))
            throw std::runtime_error("Invalid workspace UUID");

        const auto response = Get("/" + workspace + "/accounts");

        if (!response || response->getType() != JSON::JSONNode::OBJECT_NODE)
            throw std::runtime_error("Invalid accounts response");

        const auto root = std::static_pointer_cast<JSON::ObjectNode>(response);

        const auto accounts = root->getOptionalArrayNode("data");

        if (!accounts)
            throw std::runtime_error("Missing accounts data array");

        std::cout << "Accounts found: " << accounts->size() << "\n\n";

        for (const auto &entry : *accounts) {
            if (!entry || entry->getType() != JSON::JSONNode::OBJECT_NODE)
                throw std::runtime_error("Invalid account entry");

            const auto account = std::static_pointer_cast<JSON::ObjectNode>(entry);

            const std::string id = account->getOptionalStringValue("id");

            const std::string name = account->getOptionalStringValue("name");

            const std::string username = account->getOptionalStringValue("username");

            const std::string provider = account->getOptionalStringValue("provider");

            const bool authorized = account->getOptionalBooleanValue("authorized", false);

            std::cout << "ID         : " << id << '\n'
                      << "Name       : " << name << '\n'
                      << "Username   : " << username << '\n'
                      << "Provider   : " << provider << '\n'
                      << "Authorized : " << (authorized ? "yes" : "no") << "\n\n";
        }
    }
};


[[noreturn]] void CrossPostingUsage(const std::string &reason = "") {
    if (!reason.empty())
        std::cerr << "Error: " << reason << "\n\n";

    std::cerr << R"HELP(CrossPosting - Cross-posting client for Centralizing Posts Service API (ex. Mixpost).

Usage:
  cross_posting COMMAND [ARGUMENTS] [OPTIONS]
  cross_posting --config FILE COMMAND [ARGUMENTS]

Commands:
  ping                    Verify API connection
  accounts                List connected social accounts
  posts                   Get all posts with pagination
  sync                    Synchronize posts to MySQL
  draft ACCOUNT MESSAGE   Create a draft post
  --help, -h              Show this help

Options:
  --config FILE           Use an alternative configuration file
  --send                  Submit a draft to Mixpost

Default configuration:
  /usr/local/var/lib/tuelib/cross_posting.conf

Examples:
  cross_posting ping
  cross_posting ping --config /tmp/mock.conf
  cross_posting accounts --config /tmp/mock.conf
  cross_posting posts --config /tmp/mock.conf
  cross_posting sync --config /tmp/mock.conf
  cross_posting draft 2 "Test message" --config /tmp/mock.conf
  cross_posting draft 2 "Test message" --send --config /tmp/mock.conf

)HELP";

    std::exit(reason.empty() ? EXIT_SUCCESS : 2);
}


} // namespace


int Main(int argc, char *argv[]) {
    try {
        const auto options = ParseArguments(argc, argv);

        const std::string &command = options.command;

        if (command == "--help" || command == "-h") {
            if (!options.arguments.empty() || options.send)
                CrossPostingUsage("Invalid help arguments");

            CrossPostingUsage();
        }

        const bool read_command = command == "ping" || command == "accounts" || command == "posts" || command == "sync";

        const bool draft_command = command == "draft";

        if (read_command) {
            if (!options.arguments.empty() || options.send)
                CrossPostingUsage("Unexpected arguments for " + command);

        } else if (draft_command) {
            if (options.arguments.size() != 2)
                CrossPostingUsage("Draft requires ACCOUNT_ID and MESSAGE");

        } else {
            CrossPostingUsage("Unknown or missing command: " + command);
        }

        // Read the file selected by --config.
        const IniFile config(options.config_path);

        const std::string api_base = config.getString("Mixpost", "api_base_url");

        const std::string api_token = config.getString("Mixpost", "api_token");

        const CrossPostingClient client(api_base, api_token);

        if (command == "ping") {
            client.Ping();
            return EXIT_SUCCESS;
        }

        const std::string workspace = config.getString("Mixpost", "workspace_uuid");

        if (command == "accounts") {
            client.ListAccounts(workspace);

        } else if (command == "posts") {
            client.GetPosts(workspace);

        } else if (command == "sync") {
            client.SyncPosts(workspace, config);

        } else if (command == "draft") {
            const std::string &id_text = options.arguments[0];

            std::size_t parsed = 0;

            const std::int64_t account_id = std::stoll(id_text, &parsed);

            if (parsed != id_text.size() || account_id <= 0)
                throw std::runtime_error("Invalid account ID");

            const std::string &message = options.arguments[1];

            client.CreateDraft(workspace, account_id, message, options.send);
        }

        return EXIT_SUCCESS;

    } catch (const std::exception &e) {
        std::cerr << "CrossPosting error: " << e.what() << '\n';

        return EXIT_FAILURE;
    }
}