#pragma once

#include "../util/config.hpp"
#include "../util/logger.hpp"
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <toml++/toml.h>
#include <vector>

class TransferMetadata {
public:
    struct ChunkInfo {
        uint64_t index;    // 块索引
        uint64_t offset;   // 文件偏移量
        uint64_t size;     // 块大小
        bool is_completed; // 是否已完成
        std::string hash;  // 块的哈希值
    };

    TransferMetadata(const std::filesystem::path& base_dir);
    ~TransferMetadata() = default;

    // 创建新的传输元数据
    bool create(uint64_t transfer_id,
                const std::string& filename,
                uint64_t file_size,
                const std::string& file_hash,
                uint64_t chunk_size);

    // 加载现有的传输元数据
    bool load(uint64_t transfer_id);

    // 更新块状态
    bool update_chunk_status(uint64_t chunk_index, bool is_completed);

    // 获取下一个未完成的块
    std::optional<ChunkInfo> get_next_incomplete_chunk() const;

    // 检查传输是否完成
    bool is_transfer_completed() const;

    // 保存元数据到TOML文件
    bool save() const;

    // 获取元数据信息
    uint64_t get_transfer_id() const { return transfer_id_; }
    const std::string& get_filename() const { return filename_; }
    uint64_t get_file_size() const { return file_size_; }
    const std::string& get_file_hash() const { return file_hash_; }
    uint64_t get_chunk_size() const { return chunk_size_; }
    uint64_t get_completed_size() const { return completed_size_; }
    std::filesystem::path get_temp_file_path() const { return temp_file_path_; }
    std::filesystem::path get_metadata_file_path() const { return metadata_file_path_; }
    const std::vector<ChunkInfo>& get_chunks() const { return chunks_; }

private:
    // Config& config_ = Config::get_instance();
    // Logger& logger_ = Logger::get_instance();

    uint64_t transfer_id_ = 0;
    std::string filename_;
    uint64_t file_size_ = 0;
    std::string file_hash_;
    uint64_t chunk_size_ = 1024 * 1024; // 默认1MB
    uint64_t completed_size_ = 0;
    std::chrono::system_clock::time_point create_time_;
    std::chrono::system_clock::time_point update_time_;

    std::filesystem::path base_dir_;
    std::filesystem::path temp_file_path_;
    std::filesystem::path metadata_file_path_;

    std::vector<ChunkInfo> chunks_;

    // 初始化块列表
    void initialize_chunks();

    // 根据TOML更新内部状态
    void update_from_toml(const toml::table& table);

    // 构建TOML数据
    toml::table build_toml_data() const;
};