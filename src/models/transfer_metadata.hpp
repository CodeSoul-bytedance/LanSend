#pragma once

#include "../utils/logger.hpp"
#include "transfer_request.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace lansend {
namespace models {

enum class TransferStatus {
    PENDING,
    ACCEPTED,
    REJECTED,
    TRANSFERRING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

NLOHMANN_JSON_SERIALIZE_ENUM(TransferStatus,
                             {{TransferStatus::PENDING, "PENDING"},
                              {TransferStatus::ACCEPTED, "ACCEPTED"},
                              {TransferStatus::REJECTED, "REJECTED"},
                              {TransferStatus::TRANSFERRING, "TRANSFERRING"},
                              {TransferStatus::PAUSED, "PAUSED"},
                              {TransferStatus::COMPLETED, "COMPLETED"},
                              {TransferStatus::FAILED, "FAILED"},
                              {TransferStatus::CANCELLED, "CANCELLED"}});

struct ChunkInfo {
    uint64_t index;
    std::optional<std::string> hash; //sha256
    bool completed = false;
};

inline void to_json(nlohmann::json& j, const ChunkInfo& ci) {
    j = nlohmann::json{{"index", ci.index}, {"completed", ci.completed}};
    if (ci.hash.has_value()) {
        j["hash"] = ci.hash.value();
    } else {
        j["hash"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, ChunkInfo& ci) {
    j.at("index").get_to(ci.index);
    j.at("completed").get_to(ci.completed);

    if (j.contains("hash")) {
        auto const& hash_val = j.at("hash");
        if (!hash_val.is_null()) {
            ci.hash = hash_val.get<std::string>();
        } else {
            ci.hash = std::nullopt;
        }
    } else {
        ci.hash = std::nullopt;
    }
}

struct TransferMetadata {
    uint64_t transfer_id;
    std::string file_name;
    uint64_t file_size;
    std::optional<std::string> file_hash; //sha256 整个文件的哈希
    FileType file_type;
    std::optional<std::string> preview;

    std::string source_device_id;
    std::string target_device_id;

    TransferStatus status;
    uint64_t chunk_size;
    uint64_t total_chunks;
    std::vector<ChunkInfo> chunks;

    std::filesystem::path local_filepath;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;

    std::string token;

    bool is_complete() const {
        if (chunks.empty() && total_chunks > 0)
            return false; //尚未初始化或块数为0但期望有块
        if (chunks.empty() && total_chunks == 0)
            return true; // 0字节文件视为完成
        // 检查是否所有块都已完成
        return std::all_of(chunks.begin(), chunks.end(), [](const ChunkInfo& c) {
            return c.completed;
        });
    }

    double get_progress() const {
        if (total_chunks == 0)
            return is_complete() ? 1.0 : 0.0;
        if (chunks.empty())
            return 0.0; // 如果块列表为空，进度为0
        size_t completed_count = std::count_if(chunks.begin(),
                                               chunks.end(),
                                               [](const ChunkInfo& c) { return c.completed; });
        return static_cast<double>(completed_count) / total_chunks;
    }

    void mark_chunk_completed(uint64_t chunk_index) {
        // 查找对应索引的块并标记为完成
        auto it = std::find_if(chunks.begin(), chunks.end(), [chunk_index](const ChunkInfo& c) {
            return c.index == chunk_index;
        });
        if (it != chunks.end()) {
            if (!it->completed) { // 仅在状态改变时更新时间戳
                it->completed = true;
                updated_at = std::chrono::system_clock::now();
            }
        } else {
            spdlog::warn("Chunk index {} not found", chunk_index);
        }
    }

    friend void to_json(nlohmann::json& j, const TransferMetadata& tm) {
        j = nlohmann::json{{"transfer_id", tm.transfer_id},
                           {"file_name", tm.file_name},
                           {"file_size", tm.file_size},
                           {"file_hash", tm.file_hash},
                           {"file_type", tm.file_type},
                           {"preview", tm.preview},
                           {"source_device_id", tm.source_device_id},
                           {"target_device_id", tm.target_device_id},
                           {"status", tm.status},
                           {"chunk_size", tm.chunk_size},
                           {"total_chunks", tm.total_chunks},
                           {"chunks", tm.chunks},
                           {"created_at", std::chrono::system_clock::to_time_t(tm.created_at)},
                           {"updated_at", std::chrono::system_clock::to_time_t(tm.updated_at)}};
    }

    friend void from_json(const nlohmann::json& j, TransferMetadata& tm) {
        j.at("transfer_id").get_to(tm.transfer_id);
        if (j.contains("file_name"))
            j.at("file_name").get_to(tm.file_name);
        else if (j.contains("filename"))
            j.at("filename").get_to(tm.file_name);

        if (j.contains("file_size"))
            j.at("file_size").get_to(tm.file_size);
        else if (j.contains("filesize"))
            j.at("filesize").get_to(tm.file_size);

        if (j.contains("file_hash") && !j.at("file_hash").is_null())
            tm.file_hash = j.at("file_hash").get<std::string>();
        else
            tm.file_hash = std::nullopt;

        if (j.contains("file_type"))
            j.at("file_type").get_to(tm.file_type);

        if (j.contains("preview") && !j.at("preview").is_null())
            tm.preview = j.at("preview").get<std::string>();
        else
            tm.preview = std::nullopt;

        j.at("source_device_id").get_to(tm.source_device_id);
        j.at("target_device_id").get_to(tm.target_device_id);
        j.at("status").get_to(tm.status);
        j.at("chunk_size").get_to(tm.chunk_size);
        j.at("total_chunks").get_to(tm.total_chunks);

        if (j.contains("chunks")) {
            j.at("chunks").get_to(tm.chunks);
        } else {
            tm.chunks.clear(); // 没chunk
        }

        if (j.contains("local_filepath") && !j.at("local_filepath").is_null()) {
            tm.local_filepath = j.at("local_filepath").get<std::string>();
        }

        if (j.contains("created_at") && !j.at("created_at").is_null()) {
            std::time_t t = j.at("created_at").get<std::time_t>();
            tm.created_at = std::chrono::system_clock::from_time_t(t);
        } else {
            tm.created_at = std::chrono::system_clock::now();
        }

        if (j.contains("updated_at") && !j.at("updated_at").is_null()) {
            std::time_t t = j.at("updated_at").get<std::time_t>();
            tm.updated_at = std::chrono::system_clock::from_time_t(t);
        } else {
            tm.updated_at = std::chrono::system_clock::now();
        }
    }
};

} // namespace models
} // namespace lansend