#include "dedicated_protocol.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t running = 1;
volatile std::sig_atomic_t listener_socket = -1;

void Stop(int)
{
    running = 0;
    if (listener_socket >= 0) {
        close(listener_socket);
        listener_socket = -1;
    }
}

bool SendAll(int socket, const std::string& message)
{
    std::size_t sent = 0;
    while (sent < message.size()) {
        const ssize_t count = send(socket, message.data() + sent, message.size() - sent, 0);
        if (count <= 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

void Usage()
{
    std::cout << "Usage: catan-dedicated-server [--bind ADDRESS] [--port PORT] "
                 "[--max-lobbies COUNT] [--state-file PATH | --no-persistence] "
                 "[--drop-response-once OPERATION]\n";
}

bool ReadStateFile(const std::filesystem::path& path, std::string& state, std::string& error)
{
    std::error_code filesystem_error;
    if (!std::filesystem::exists(path, filesystem_error)) {
        if (filesystem_error) error = "Could not inspect state file: " + filesystem_error.message();
        return !filesystem_error;
    }
    const std::uintmax_t size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) { error = "Could not inspect state file: " + filesystem_error.message(); return false; }
    if (size > 64 * 1024 * 1024) { error = "Dedicated server state file is too large"; return false; }
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "Could not open dedicated server state file"; return false; }
    state.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (input.bad()) { error = "Could not read dedicated server state file"; return false; }
    return true;
}

bool WriteStateFile(const std::filesystem::path& path, std::string_view state, std::string& error)
{
    std::error_code filesystem_error;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) { error = "Could not create state directory: " + filesystem_error.message(); return false; }
    }
    const std::filesystem::path temporary = path.string() + ".tmp";
    const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW,
        S_IRUSR | S_IWUSR);
    if (descriptor < 0) { error = "Could not open temporary state file: " + std::string(std::strerror(errno)); return false; }
    if (fchmod(descriptor, S_IRUSR | S_IWUSR) != 0) {
        error = "Could not secure temporary state file: " + std::string(std::strerror(errno));
        close(descriptor); unlink(temporary.c_str()); return false;
    }
    std::size_t offset = 0;
    while (offset < state.size()) {
        const ssize_t count = write(descriptor, state.data() + offset, state.size() - offset);
        if (count <= 0) {
            error = "Could not write dedicated server state: " + std::string(std::strerror(errno));
            close(descriptor); unlink(temporary.c_str()); return false;
        }
        offset += static_cast<std::size_t>(count);
    }
    const int flush_result = fsync(descriptor);
    const int flush_error = errno;
    const int close_result = close(descriptor);
    if (flush_result != 0 || close_result != 0) {
        error = "Could not flush dedicated server state: "
            + std::string(std::strerror(flush_result != 0 ? flush_error : errno));
        unlink(temporary.c_str()); return false;
    }
    if (rename(temporary.c_str(), path.c_str()) != 0) {
        error = "Could not publish dedicated server state: " + std::string(std::strerror(errno));
        unlink(temporary.c_str()); return false;
    }
    return true;
}

std::string_view RequestOperation(std::string_view request)
{
    while (!request.empty() && (request.back() == '\n' || request.back() == '\r')) request.remove_suffix(1);
    const std::size_t separator = request.find('\t');
    return request.substr(0, separator);
}

bool IsSuccessfulMutation(std::string_view request, std::string_view response)
{
    if (!response.starts_with("OK\t")) return false;
    const std::string_view operation = RequestOperation(request);
    return operation == "CREATE" || operation == "JOIN" || operation == "LEAVE"
        || operation == "READY" || operation == "START" || operation == "COMMAND"
        || operation == "CREATE2" || operation == "JOIN2" || operation == "LEAVE2"
        || operation == "READY2" || operation == "START2" || operation == "COMMAND2";
}

} // namespace

int main(int argc, char** argv)
{
    std::string bind_address = "0.0.0.0";
    std::optional<std::filesystem::path> state_file = std::filesystem::path("catan-dedicated.state");
    std::string drop_response_once;
    bool response_dropped = false;
    int port = 17777;
    int max_lobbies = 128;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") { Usage(); return 0; }
        if (argument == "--version") { std::cout << "catan-dedicated-server " CATAN_VERSION "\n"; return 0; }
        if ((argument == "--bind" || argument == "--port" || argument == "--max-lobbies"
            || argument == "--state-file" || argument == "--drop-response-once")
            && index + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n'; return 2;
        }
        try {
            if (argument == "--bind") bind_address = argv[++index];
            else if (argument == "--port") port = std::stoi(argv[++index]);
            else if (argument == "--max-lobbies") max_lobbies = std::stoi(argv[++index]);
            else if (argument == "--state-file") state_file = std::filesystem::path(argv[++index]);
            else if (argument == "--drop-response-once") drop_response_once = argv[++index];
            else if (argument == "--no-persistence") state_file.reset();
            else { std::cerr << "Unknown option: " << argument << '\n'; return 2; }
        } catch (...) { std::cerr << "Invalid value for " << argument << '\n'; return 2; }
    }
    if (port < 0 || port > 65535 || max_lobbies < 1) {
        std::cerr << "Port or lobby limit is out of range\n"; return 2;
    }
    if (state_file && state_file->empty()) {
        std::cerr << "State file path cannot be empty\n"; return 2;
    }
    if (drop_response_once.size() > 32) {
        std::cerr << "Dropped-response operation is invalid\n"; return 2;
    }

    ivv::catan::dedicated::Service service(static_cast<std::size_t>(max_lobbies));
    if (state_file) {
        std::string state;
        std::string error;
        if (!ReadStateFile(*state_file, state, error)) {
            std::cerr << error << '\n'; return 1;
        }
        if (!state.empty()) {
            const auto restored = service.RestoreState(state);
            if (!restored.ok) {
                std::cerr << "Could not restore " << state_file->string() << ": "
                          << restored.message << '\n';
                return 1;
            }
        }
    }

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { std::cerr << "socket: " << std::strerror(errno) << '\n'; return 1; }
    listener_socket = listener;
    int reuse = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, bind_address.c_str(), &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 bind address\n"; close(listener); return 2;
    }
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || listen(listener, 32) != 0) {
        std::cerr << "listen: " << std::strerror(errno) << '\n'; close(listener); return 1;
    }
    socklen_t address_size = sizeof(address);
    getsockname(listener, reinterpret_cast<sockaddr*>(&address), &address_size);
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    std::cout << "CATAN_DEDICATED_READY address=" << bind_address
              << " port=" << ntohs(address.sin_port)
              << " lobbies=" << service.LobbyCount()
              << " persistence=" << (state_file ? state_file->string() : "off") << std::endl;

    while (running) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
            if (!running) break;
            if (errno == EINTR) continue;
            std::cerr << "accept: " << std::strerror(errno) << '\n'; break;
        }
        timeval timeout{5, 0};
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        std::string request;
        char buffer[4096];
        while (request.size() <= 1024 * 1024 && request.find('\n') == std::string::npos) {
            const ssize_t count = recv(client, buffer, sizeof(buffer), 0);
            if (count <= 0) break;
            request.append(buffer, static_cast<std::size_t>(count));
        }
        std::string response;
        if (request.size() > 1024 * 1024)
            response = "ERR\t5265717565737420697320746f6f206c61726765";
        else
            response = ivv::catan::dedicated::protocol::HandleRequest(service, request);
        if (state_file && IsSuccessfulMutation(request, response)) {
            std::string persistence_error;
            try {
                if (!WriteStateFile(*state_file, service.SerializeState(), persistence_error)) {
                    std::cerr << persistence_error << '\n';
                    response = "ERR\t50657273697374656e6365206661696c6564";
                    running = 0;
                }
            } catch (const std::exception& exception) {
                std::cerr << "Could not serialize dedicated server state: " << exception.what() << '\n';
                response = "ERR\t50657273697374656e6365206661696c6564";
                running = 0;
            }
        }
        if (!response_dropped && !drop_response_once.empty()
            && RequestOperation(request) == drop_response_once) {
            response_dropped = true;
            std::cout << "CATAN_DEDICATED_DEBUG dropped_response operation="
                      << drop_response_once << std::endl;
            close(client);
            continue;
        }
        response.push_back('\n');
        SendAll(client, response);
        close(client);
    }
    if (listener_socket >= 0) {
        close(listener_socket);
        listener_socket = -1;
    }
    if (state_file) {
        std::string error;
        try {
            if (!WriteStateFile(*state_file, service.SerializeState(), error)) {
                std::cerr << error << '\n'; return 1;
            }
        } catch (const std::exception& exception) {
            std::cerr << "Could not serialize dedicated server state: " << exception.what() << '\n';
            return 1;
        }
    }
    return 0;
}
