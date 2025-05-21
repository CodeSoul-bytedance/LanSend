#include <core/security/file_hasher.h>
#include <core/security/open_ssl_provider.h>
#include <fstream>

namespace lansend::core {

std::string FileHasher::CalculateFileChecksum(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file for checksum calculation");
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);

    constexpr size_t buffer_size = 8192;
    std::vector<char> buffer(buffer_size);

    while (file) {
        file.read(buffer.data(), buffer_size);
        size_t bytes_read = file.gcount();
        if (bytes_read > 0) {
            EVP_DigestUpdate(mdctx, buffer.data(), bytes_read);
        }
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    // Convert hash to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

std::string FileHasher::CalculateDataChecksum(const BinaryData& data) {
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(mdctx, data.data(), data.size());

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

FileHasher::FileHasher() {
    OpenSSLProvider::InitOpenSSL();
}

} // namespace lansend::core