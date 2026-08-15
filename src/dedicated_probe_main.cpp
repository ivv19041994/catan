#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv)
{
    std::string host = "127.0.0.1";
    std::string port = "17777";
    std::string request;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if ((argument == "--host" || argument == "--port" || argument == "--request") && index + 1 < argc) {
            if (argument == "--host") host = argv[++index];
            else if (argument == "--port") port = argv[++index];
            else request = argv[++index];
        } else {
            std::cerr << "Usage: catan-dedicated-probe --host HOST --port PORT --request REQUEST\n";
            return 2;
        }
    }
    if (request.empty()) { std::cerr << "Request is required\n"; return 2; }
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    const int lookup = getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses);
    if (lookup != 0) { std::cerr << gai_strerror(lookup) << '\n'; return 1; }
    int connection = -1;
    for (addrinfo* address = addresses; address; address = address->ai_next) {
        connection = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (connection >= 0 && connect(connection, address->ai_addr, address->ai_addrlen) == 0) break;
        if (connection >= 0) close(connection);
        connection = -1;
    }
    freeaddrinfo(addresses);
    if (connection < 0) { std::cerr << "Could not connect to dedicated server\n"; return 1; }
    request.push_back('\n');
    std::size_t sent = 0;
    while (sent < request.size()) {
        const ssize_t count = send(connection, request.data() + sent, request.size() - sent, 0);
        if (count <= 0) { std::cerr << std::strerror(errno) << '\n'; close(connection); return 1; }
        sent += static_cast<std::size_t>(count);
    }
    std::string response; char buffer[4096];
    while (response.find('\n') == std::string::npos) {
        const ssize_t count = recv(connection, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        response.append(buffer, static_cast<std::size_t>(count));
    }
    close(connection);
    if (!response.empty() && response.back() == '\n') response.pop_back();
    std::cout << response << '\n';
    return response.rfind("OK\t", 0) == 0 ? 0 : 3;
}
