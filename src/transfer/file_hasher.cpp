// src/transfer/file_hasher.cpp
#include "file_hasher.hpp"
#include <fstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sstream>

struct FileHasher::OpenSSLContext {
    EVP_MD_CTX* md_ctx_ = nullptr;
    const EVP_MD* md_type_ = nullptr;

    OpenSSLContext() {
        md_type_ = EVP_sha256();
        md_ctx_ = EVP_MD_CTX_new();
        if (!md_ctx_) {
            spdlog::error("FileHasher: Failed to create EVP_MD_CTX.");
        }
    }

    ~OpenSSLContext() {
        if (md_ctx_) {
            EVP_MD_CTX_free(md_ctx_);
            md_ctx_ = nullptr;
        }
    }

    OpenSSLContext(const OpenSSLContext&) = delete;
    OpenSSLContext& operator=(const OpenSSLContext&) = delete;
    OpenSSLContext(OpenSSLContext&&) = delete;
    OpenSSLContext& operator=(OpenSSLContext&&) = delete;

    bool init() {
        if (!md_ctx_ || !md_type_) {
            spdlog::error(
                "FileHasher: OpenSSL context or MD type not properly initialized before init.");
            return false;
        }
        return EVP_DigestInit_ex(md_ctx_, md_type_, nullptr) == 1;
    }

    bool update(const unsigned char* data, size_t len) {
        if (!md_ctx_) {
            spdlog::error("FileHasher: OpenSSL context not initialized for update.");
            return false;
        }
        return EVP_DigestUpdate(md_ctx_, data, len) == 1;
    }

    std::string finalize() {
        if (!md_ctx_) {
            spdlog::error("FileHasher: OpenSSL context not initialized for finalize.");
            return "";
        }
        unsigned char hash[SHA256_DIGEST_LENGTH];
        unsigned int hash_len = 0;
        if (EVP_DigestFinal_ex(md_ctx_, hash, &hash_len) != 1) {
            spdlog::error(
                "FileHasher: Failed to finalize SHA256 hash (EVP_DigestFinal_ex failed).");
            return "";
        }
        if (hash_len != SHA256_DIGEST_LENGTH) {
            spdlog::error(
                "FileHasher: Finalized hash length ({}) does not match SHA256_DIGEST_LENGTH ({}).",
                hash_len,
                SHA256_DIGEST_LENGTH);
            return "";
        }

        std::stringstream ss;
        for (unsigned int i = 0; i < hash_len; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }
};

FileHasher::FileHasher()
    : openssl_context_(std::make_unique<OpenSSLContext>()) {
    if (!openssl_context_ || !openssl_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSLContext unique_ptr is valid, but internal md_ctx_ is "
                      "null. Construction failed.");
        openssl_context_.reset();
    }
}

FileHasher::~FileHasher() = default;

std::string FileHasher::calculate_sha256_sync(const std::filesystem::path& filepath) {
    if (!openssl_context_ || !openssl_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSL context not available for sync calculation (nullptr or "
                      "md_ctx_ is null).");
        return "";
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("FileHasher: Failed to open file for hashing: {}", filepath.string());
        return "";
    }

    if (!openssl_context_->init()) {
        spdlog::error("FileHasher: Failed to initialize SHA256 context for sync calculation.");
        return "";
    }

    const int buffer_size = 16 * 1024 * 1024;
    std::unique_ptr<char[]> buffer = std::make_unique<char[]>(buffer_size);
    while (file.good()) {
        file.read(buffer.get(), buffer_size);
        std::streamsize bytes_read = file.gcount();
        if (bytes_read > 0) {
            if (!openssl_context_->update(reinterpret_cast<const unsigned char*>(buffer.get()),
                                          static_cast<size_t>(bytes_read))) {
                spdlog::error("FileHasher: Failed to update SHA256 hash during sync calculation.");
                return "";
            }
        }
    }

    if (file.bad() && !file.eof()) {
        spdlog::error("FileHasher: Error reading file during hashing: {}", filepath.string());
        return "";
    }

    return openssl_context_->finalize();
}

bool FileHasher::start_chunked_sha256() {
    if (!openssl_context_ || !openssl_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSL context not available for chunked calculation start "
                      "(nullptr or md_ctx_ is null).");
        return false;
    }
    return openssl_context_->init();
}

bool FileHasher::update_chunk(const std::vector<uint8_t>& chunk) {
    if (!openssl_context_ || !openssl_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSL context not available for chunk update (nullptr or "
                      "md_ctx_ is null).");
        return false;
    }
    if (chunk.empty()) {
        spdlog::warn("FileHasher: update_chunk called with empty chunk.");
        return true;
    }
    return openssl_context_->update(chunk.data(), chunk.size());
}

std::string FileHasher::finalize_chunked_sha256() {
    if (!openssl_context_ || !openssl_context_->md_ctx_) {
        spdlog::error("FileHasher: OpenSSL context not available for chunked finalize (nullptr or "
                      "md_ctx_ is null).");
        return "";
    }
    return openssl_context_->finalize();
}