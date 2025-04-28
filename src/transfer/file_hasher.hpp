#pragma once

#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class FileHasher {
public:
    FileHasher();
    ~FileHasher();

    // 同步哈希计算
    std::string calculate_sha256_sync(const std::filesystem::path& filepath);

    // 分块哈希计算
    bool start_chunked_sha256();
    bool update_chunk(const std::vector<uint8_t>& chunk);
    std::string finalize_chunked_sha256();

private:
    Config& config_;
    Logger& logger_;

    // OpenSSL相关
    struct OpenSSLContext;
    std::unique_ptr<OpenSSLContext> openssl_context_;
};