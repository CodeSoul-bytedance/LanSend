#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace lansend {

using BinaryData = std::vector<std::uint8_t>;
using BinaryMessage = std::vector<std::uint8_t>;

namespace details {

struct BinaryHeader {
    std::uint32_t metadata_size;
};

} // namespace details

inline BinaryMessage CreateBinaryMessage(const nlohmann::json& metadata, const BinaryData& data) {
    BinaryMessage message;
    message.resize(sizeof(details::BinaryHeader) + metadata.dump().size() + data.size());

    details::BinaryHeader header;
    header.metadata_size = htonl(static_cast<std::uint32_t>(metadata.dump().size()));

    std::memcpy(message.data(), &header, sizeof(header));
    std::memcpy(message.data() + sizeof(header), metadata.dump().data(), metadata.dump().size());
    if (!data.empty()) {
        std::memcpy(message.data() + sizeof(header) + metadata.dump().size(),
                    data.data(),
                    data.size());
    }

    return message;
}

inline bool ParseBinaryMessage(const BinaryMessage& message,
                               nlohmann::json& metadata,
                               BinaryData& data) {
    if (message.size() < sizeof(details::BinaryHeader)) {
        return false;
    }

    details::BinaryHeader header;
    std::memcpy(&header, message.data(), sizeof(header));

    std::uint32_t metadata_size = ntohl(header.metadata_size);

    if (message.size() < sizeof(header) + metadata_size) {
        return false;
    }

    std::string metadata_str(reinterpret_cast<const char*>(message.data() + sizeof(header)),
                             metadata_size);
    try {
        metadata = nlohmann::json::parse(metadata_str);
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("Failed to parse metadata JSON: {}", e.what());
        return false;
    }

    size_t data_size = message.size() - sizeof(header) - metadata_size;
    if (data_size > 0) {
        data.resize(data_size);
        std::memcpy(data.data(), message.data() + sizeof(header) + metadata_size, data_size);
    } else {
        data.clear();
    }

    return true;
}

} // namespace lansend
