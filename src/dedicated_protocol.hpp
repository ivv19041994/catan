#pragma once

#include "dedicated_server.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ivv::catan::dedicated::protocol {

std::string HexEncode(std::string_view value);
std::optional<std::string> HexDecode(std::string_view value);
std::vector<std::string> Split(std::string_view value, char delimiter);
std::string SerializeSnapshot(const Snapshot& snapshot);
std::optional<Snapshot> DeserializeSnapshot(std::string_view payload, std::string& error);
std::string HandleRequest(Service& service, std::string_view request);

} // namespace ivv::catan::dedicated::protocol
