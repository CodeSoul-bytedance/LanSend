#pragma once

#include <core/util/binary_message.h>
#include <filesystem>

namespace lansend::core {

class FileHasher {
public:
    static std::string CalculateFileChecksum(const std::filesystem::path& file_path);
    static std::string CalculateDataChecksum(const BinaryData& data);

private:
    FileHasher();
    static FileHasher instance;
};

} // namespace lansend::core