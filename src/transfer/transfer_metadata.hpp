// #pragma once

// #include <cstdint>
// #include <filesystem>
// #include <optional>
// #include <string>
// #include <toml++/toml.hpp>
// #include <vector>

// namespace lansend {

// struct ChunkInfo {
//     uint64_t index;
//     std::string hash;
//     bool completed = false;
// };

// class TransferMetadata {
// public:
//     // Constructor (consider parameters needed for creation/loading)
//     TransferMetadata(uint64_t transfer_id,
//                      const std::string& filename,
//                      uint64_t file_size,
//                      const std::string& file_hash,
//                      uint64_t chunk_size);

//     // Factory method or similar to create new metadata and potentially save it initially
//     static std::optional<TransferMetadata> create(const std::filesystem::path& metadata_dir,
//                                                   uint64_t transfer_id,
//                                                   const std::string& filename,
//                                                   uint64_t file_size,
//                                                   const std::string& file_hash,
//                                                   uint64_t chunk_size);

//     // Load metadata from persistent storage (e.g., TOML file)
//     static std::optional<TransferMetadata> load(const std::filesystem::path& metadata_dir,
//                                                 uint64_t transfer_id);

//     // Save metadata to persistent storage
//     bool save(const std::filesystem::path& metadata_dir) const;

//     // Update the status of a specific chunk
//     bool update_chunk_status(uint64_t chunk_index, bool completed);

//     // Find the index of the next chunk that needs to be transferred
//     std::optional<uint64_t> get_next_incomplete_chunk_index() const;

//     // --- Getters ---
//     uint64_t get_transfer_id() const;
//     const std::string& get_filename() const;
//     uint64_t get_file_size() const;
//     const std::string& get_file_hash() const;
//     uint64_t get_chunk_size() const;
//     uint64_t get_completed_size() const; // Needs calculation based on chunks
//     const std::vector<ChunkInfo>& get_chunks() const;
//     uint64_t get_total_chunks() const; // Needs calculation

// private:
//     // Calculate total chunks based on file size and chunk size
//     void calculate_initial_chunks();
//     // Helper to get the path to the metadata file
//     static std::filesystem::path get_metadata_filepath(const std::filesystem::path& metadata_dir,
//                                                        uint64_t transfer_id);

//     uint64_t transfer_id_;
//     std::string filename_;
//     uint64_t file_size_;
//     std::string file_hash_;
//     uint64_t chunk_size_;
//     uint64_t completed_size_ = 0; // Track completed size
//     std::vector<ChunkInfo> chunks_;
//     // Add other necessary members, e.g., transfer status (pending, active, completed, failed)
// };

// } // namespace lansend