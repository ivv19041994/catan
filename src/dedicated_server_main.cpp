#include "dedicated_protocol.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::atomic<bool> running = true;

void Stop(int) { running = false; }

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
                 "[--max-lobbies COUNT]\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::string bind_address = "0.0.0.0";
    int port = 17777;
    int max_lobbies = 128;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help") { Usage(); return 0; }
        if ((argument == "--bind" || argument == "--port" || argument == "--max-lobbies")
            && index + 1 >= argc) {
            std::cerr << "Missing value for " << argument << '\n'; return 2;
        }
        try {
            if (argument == "--bind") bind_address = argv[++index];
            else if (argument == "--port") port = std::stoi(argv[++index]);
            else if (argument == "--max-lobbies") max_lobbies = std::stoi(argv[++index]);
            else { std::cerr << "Unknown option: " << argument << '\n'; return 2; }
        } catch (...) { std::cerr << "Invalid value for " << argument << '\n'; return 2; }
    }
    if (port < 0 || port > 65535 || max_lobbies < 1) {
        std::cerr << "Port or lobby limit is out of range\n"; return 2;
    }

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { std::cerr << "socket: " << std::strerror(errno) << '\n'; return 1; }
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
    ivv::catan::dedicated::Service service(static_cast<std::size_t>(max_lobbies));
    std::cout << "CATAN_DEDICATED_READY address=" << bind_address
              << " port=" << ntohs(address.sin_port) << std::endl;

    while (running) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
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
        response.push_back('\n');
        SendAll(client, response);
        close(client);
    }
    close(listener);
    return 0;
}
