/**
 * @author Alejandro Feo Martín, Clemente Feo González
 * @date July 2, 2024
 * @brief WebSocket server for handling messages and topic subscriptions
 *
 * This program implements a WebSocket server that allows clients to
 * subscribe to topics and send/receive files as messages. It supports configuration
 * via file and command line, and logs activities to a log file or syslog.
 */
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <syslog.h>
#include <ctime>
#include <regex>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// Custom configuration for the WebSocket server
struct custom_config : public websocketpp::config::asio {
    typedef custom_config type;
    typedef websocketpp::config::asio base;

    static const websocketpp::log::level alog_level = websocketpp::log::alevel::none;
    static const websocketpp::log::level elog_level = websocketpp::log::elevel::none;

    static size_t max_message_size;
};

// Default maximum message size (200 MB)
size_t custom_config::max_message_size = 200 * 1024 * 1024;

// WebSocket server type with custom configuration
typedef websocketpp::server<custom_config> server;

// Map of topics and their subscribers
std::map<std::string, std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>>> topics;
// Map of connections and their associated topics
std::map<websocketpp::connection_hdl, std::string, std::owner_less<websocketpp::connection_hdl>> connections;
// Map of connections and client IP addresses
std::map<websocketpp::connection_hdl, std::string, std::owner_less<websocketpp::connection_hdl>> client_ips;
// Log file path
std::string log_file_path;

/**
 * @brief Logs a message to the log file or syslog
 * @param message The message to be logged
 */
void log_message(const std::string& message) {
    std::time_t now = std::time(nullptr);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%b %d %H:%M:%S", std::localtime(&now));

    std::string formatted_message = std::string(timestamp) + " websocket_server: " + message + "\n";

    if (!log_file_path.empty()) {
        std::ofstream log_file(log_file_path, std::ios_base::app);
        if (log_file.is_open()) {
            log_file << formatted_message;
            log_file.close();
        } else {
            std::cerr << "Failed to open log file: " << log_file_path << std::endl;
        }
    } else {
        openlog("websocket_server", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_INFO, "%s", message.c_str());
        closelog();
    }
}

/**
 * @brief Checks if a port is available for use
 * @param port The port number to check
 * @param ip The IP address on which to attempt binding the port
 * @return true if the port is available, false otherwise
 */
bool is_port_available(int port, const std::string& ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip == "0.0.0.0" ? INADDR_ANY : inet_addr(ip.c_str());

    int result = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);

    return result == 0;
}


/**
 * @brief Handles messages received by the WebSocket server
 * @param s Pointer to the WebSocket server
 * @param hdl Connection handler
 * @param msg Pointer to the received message
 */
void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    std::string client_ip = client_ips[hdl];
    std::string payload = msg->get_payload();

    if (payload.substr(0, 10) == "subscribe:") {
        std::string topic = payload.substr(10);
        topics[topic].insert(hdl);
        connections[hdl] = topic;
        std::cout << "Client subscribed to topic [" << topic << "]." << std::endl;
        log_message("Client subscribed to topic [" + topic + "]");
    } else if (payload.substr(0, 8) == "message:") {
        size_t pos = payload.find(':', 8);
        if (pos != std::string::npos) {
            std::string topic = payload.substr(8, pos - 8);
            std::string message = payload.substr(pos + 1);

            // Extraer el tipo MIME y el nombre del archivo
            size_t mime_pos = message.find(':');
            size_t file_pos = message.find(':', mime_pos + 1);
            if (mime_pos != std::string::npos && file_pos != std::string::npos) {
                std::string mime_type = message.substr(0, mime_pos);
                std::string file_name = message.substr(mime_pos + 1, file_pos - mime_pos - 1);
                
                std::cout << "Message received from IP [" << client_ip << "] on topic [" << topic << "]: MIME: " << mime_type << ", File: " << file_name << std::endl;
                log_message("Message received from IP [" + client_ip + "] on topic [" + topic + "]: MIME: " + mime_type + ", File: " + file_name);
            } else {
                std::cout << "Message received on topic [" << topic << "]: Invalid format" << std::endl;
                log_message("Message received on topic [" + topic + "]: Invalid format");
            }

            for (auto& conn : topics[topic]) {
                if (conn.lock() != hdl.lock()) {
                    websocketpp::lib::error_code ec;
                    s->send(conn, message, msg->get_opcode(), ec);
                    if (ec) {
                        std::string error_msg = "Error forwarding message: " + ec.message();
                        std::cout << error_msg << std::endl;
                        log_message(error_msg);
                    } else {
                        log_message("Message forwarded on topic [" + topic + "]");
                    }
                }
            }
        }
    }
}

/**
 * @brief Handles new WebSocket connection openings
 * @param s Pointer to the WebSocket server
 * @param hdl Connection handler
 */
void on_open(server* s, websocketpp::connection_hdl hdl) {
    websocketpp::lib::error_code ec;
    auto con = s->get_con_from_hdl(hdl, ec);
    if (ec) {
        std::cout << "Error getting connection: " << ec.message() << std::endl;
        log_message("Error getting connection: " + ec.message());
        return;
    }

    std::string client_ip = con->get_remote_endpoint();
    client_ips[hdl] = client_ip;

    std::cout << "Connection opened from IP: " << client_ip << std::endl;
    log_message("Connection opened from IP: " + client_ip);
}

/**
 * @brief Handles WebSocket connection closures
 * @param s Pointer to the WebSocket server
 * @param hdl Connection handler
 */
void on_close(server* s, websocketpp::connection_hdl hdl) {
    auto it = client_ips.find(hdl);
    if (it != client_ips.end()) {
        std::cout << "Connection closed from IP: " << it->second << std::endl;
        log_message("Connection closed from IP: " + it->second);
        client_ips.erase(it);
    } else {
        std::cout << "Connection closed from unknown IP" << std::endl;
        log_message("Connection closed from unknown IP");
    }

    auto conn_it = connections.find(hdl);
    if (conn_it != connections.end()) {
        topics[conn_it->second].erase(hdl);
        if (topics[conn_it->second].empty()) {
            topics.erase(conn_it->second);
        }
        connections.erase(conn_it);
    }
}


/**
 * @brief Displays the program's help information
 * @param program_name Name of the executable program
 */
void show_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p <port>             Specify the port to use (default: 8765)." << std::endl;
    std::cout << "  -i <IP>               Specify the IP address to listen on (default: all IPs)." << std::endl;
    std::cout << "  -s <max size in MB>   Specify the maximum message size in MB (default: 200 MB)." << std::endl;
    std::cout << "  -l <log file path>    Specify the path for the log file (default: syslog)." << std::endl;
    std::cout << "  -c <config file>      Specify a custom configuration file." << std::endl;
    std::cout << "  -h                    Show this help." << std::endl;
}

/**
 * @brief Checks if a string is a valid IP address
 * @param ip The string to check
 * @return true if it's a valid IP address, false otherwise
 */
bool is_valid_ip(const std::string& ip) {
    std::regex ipv4_regex(
        "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}"
        "(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    return std::regex_match(ip, ipv4_regex);
}

/**
 * @brief Checks if an IP address is assigned to a network interface
 * @param ip The IP address to check
 * @return true if the IP is assigned, false otherwise
 */
bool is_ip_assigned(const std::string& ip) {
    if (ip == "0.0.0.0") {
        return true;  // Always allow 0.0.0.0
    }

    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return false;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                freeifaddrs(ifaddr);
                return false;
            }

            if (ip == host) {
                freeifaddrs(ifaddr);
                return true;
            }
        }
    }

    freeifaddrs(ifaddr);
    return false;
}

/**
 * @brief Checks if a file exists and is readable
 * @param filepath Path to the file to check
 * @return true if the file exists and is readable, false otherwise
 */
bool is_file_readable(const std::string& filepath) {
    std::ifstream file(filepath);
    return file.good();
}

/**
 * @brief Checks if a directory is writable
 * @param dirpath Path to the directory to check
 * @return true if the directory is writable, false otherwise
 */
bool is_directory_writable(const std::string& dirpath) {
    std::string testfile = dirpath + "/test_write_permission.tmp";
    std::ofstream file(testfile);
    if (file.good()) {
        file.close();
        std::remove(testfile.c_str());
        return true;
    }
    return false;
}

/**
 * @brief Loads configuration from a file
 * @param config_file Path to the configuration file
 * @param port Reference to the port to configure
 * @param ip Reference to the IP address to configure
 * @param max_message_size Reference to the maximum message size to configure
 * @param log_file_path Reference to the log file path to configure
 */
void load_config(const std::string& config_file, int& port, std::string& ip, size_t& max_message_size, std::string& log_file_path) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open configuration file: " << config_file << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;  // Skip empty lines and comments

        size_t delimiter_pos = line.find('=');
        if (delimiter_pos == std::string::npos) continue;  // Skip invalid lines

        std::string key = line.substr(0, delimiter_pos);
        std::string value = line.substr(delimiter_pos + 1);

        if (key == "port") {
            try {
                port = std::stoi(value);
            } catch (const std::exception& e) {
                std::cerr << "Invalid port in config file: " << value << std::endl;
            }
        } else if (key == "ip") {
            ip = value;
        } else if (key == "max_message_size") {
            try {
                max_message_size = std::stoull(value) * 1024 * 1024;
            } catch (const std::exception& e) {
                std::cerr << "Invalid max message size in config file: " << value << std::endl;
            }
        } else if (key == "log_file") {
            log_file_path = value;
        }
    }
}

/**
 * @brief Main function of the program
 * @param argc Number of command-line arguments
 * @param argv Array of command-line arguments
 * @return 0 if the program runs successfully, non-zero value otherwise
 */
int main(int argc, char* argv[]) {
    int port = 8765;
    std::string ip = "0.0.0.0";
    std::string config_file = "/etc/websocket_server/websocket_server.conf";

    // First, check if a custom config file is specified
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-c" && i + 1 < argc) {
            config_file = argv[i + 1];
            break;
        }
    }

    // Load configuration from file
    load_config(config_file, port, ip, custom_config::max_message_size, log_file_path);

    // Then, process command line arguments (they override config file settings)
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
            show_help(argv[0]);
            return 0;
        } else if (std::string(argv[i]) == "-p" && i + 1 < argc) {
            // Port validation (already implemented)
        } else if (std::string(argv[i]) == "-i" && i + 1 < argc) {
            // IP validation (already implemented)
        } else if (std::string(argv[i]) == "-s" && i + 1 < argc) {
            try {
                size_t max_size = std::stoull(argv[i + 1]);
                if (max_size == 0 || max_size > 1024) {  // Assuming a max limit of 1024 MB
                    throw std::out_of_range("Invalid message size");
                }
                custom_config::max_message_size = max_size * 1024 * 1024;
            } catch (const std::exception& e) {
                std::cerr << "Invalid maximum message size: " << argv[i + 1] << ". Error: " << e.what() << std::endl;
                return 1;
            }
        } else if (std::string(argv[i]) == "-l" && i + 1 < argc) {
            log_file_path = argv[i + 1];
            std::string dir = log_file_path.substr(0, log_file_path.find_last_of("/\\"));
            if (!is_directory_writable(dir)) {
                std::cerr << "Log file directory is not writable: " << dir << std::endl;
                return 1;
            }
        } else if (std::string(argv[i]) == "-c" && i + 1 < argc) {
            config_file = argv[i + 1];
            if (!is_file_readable(config_file)) {
                std::cerr << "Configuration file is not readable: " << config_file << std::endl;
                return 1;
            }
        }
    }

    // Load configuration from file
    if (!config_file.empty()) {
        load_config(config_file, port, ip, custom_config::max_message_size, log_file_path);
    }

    // Validate IP and port after all settings have been processed
    if (!is_valid_ip(ip)) {
        std::cerr << "Invalid IP address: " << ip << std::endl;
        return 1;
    }
    if (!is_ip_assigned(ip)) {
        std::cerr << "IP address " << ip << " is not assigned to any network interface." << std::endl;
        return 1;
    }
    if (!is_port_available(port, ip)) {
        std::cerr << "Port " << port << " is already in use or not available." << std::endl;
        return 1;
    }

    server s;

    s.set_max_message_size(custom_config::max_message_size);

    std::cout << "Max message size set to: " << custom_config::max_message_size / 1024 / 1024 << " MB." << std::endl;
    std::cout << "\033[1;33m" << "IP: " << ip << "\033[0m" << std::endl << "\033[1;33m" << "Port: " << port << "\033[0m" << std::endl;
    log_message("Server started. IP: " + ip + ", Port: " + std::to_string(port) + ", Max message size: " + std::to_string(custom_config::max_message_size / 1024 / 1024) + " MB");

    s.init_asio();
    s.set_open_handler(bind(&on_open, &s, std::placeholders::_1));
    s.set_close_handler(bind(&on_close, &s, std::placeholders::_1));
    s.set_message_handler(bind(&on_message, &s, std::placeholders::_1, std::placeholders::_2));

    websocketpp::lib::asio::ip::tcp::endpoint endpoint;
    if (ip == "0.0.0.0") {
        endpoint = websocketpp::lib::asio::ip::tcp::endpoint(websocketpp::lib::asio::ip::tcp::v4(), port);
    } else {
        endpoint = websocketpp::lib::asio::ip::tcp::endpoint(websocketpp::lib::asio::ip::address::from_string(ip), port);
    }

    s.set_reuse_addr(true);
    s.listen(endpoint);
    s.start_accept();
    s.run();

    return 0;
}
