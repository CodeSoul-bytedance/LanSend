#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

namespace lansend::core {

struct ReceiveFileContext {
    std::string file_name;                           // 文件名
    std::filesystem::path temp_file_path;            // 临时文件路径
    std::string file_token;                          // 文件令牌
    size_t file_size;                                // 文件总大小
    size_t chunk_size;                               // 块大小
    size_t total_chunks;                             // 总块数
    std::unordered_set<std::size_t> received_chunks; // 已接收块集合
    std::string file_checksum;                       // 整个文件的校验和
};

} // namespace lansend::core
