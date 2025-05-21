#pragma once

#include "../file_type.h"
#include <nlohmann/json.hpp>
#include <string>

namespace lansend::core {

struct FileDto {
    std::string file_id;       // 文件唯一标识符
    std::string file_name;     // 文件名
    size_t file_size;          // 文件总大小
    size_t chunk_size;         // 块大小
    size_t total_chunks;       // 总块数
    std::string file_checksum; // 整个文件的校验和
    FileType file_type;        // 文件类型

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        FileDto, file_id, file_name, file_size, chunk_size, total_chunks, file_checksum, file_type);
};

} // namespace lansend::core