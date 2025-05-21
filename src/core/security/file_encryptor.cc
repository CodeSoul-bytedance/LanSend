#include <core/security/file_encryptor.h>
#include <core/security/open_ssl_provider.h>
#include <cstring>
#include <fstream>
#include <spdlog/spdlog.h>

namespace lansend::core {

using ErrorType = std::string;

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::GenerateKey(int key_size) {
    std::vector<uint8_t> key(key_size);
    if (RAND_bytes(key.data(), key_size) != 1) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
    return key;
}

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::GenerateIV() {
    std::vector<std::uint8_t> iv(IV_SIZE);
    if (RAND_bytes(iv.data(), IV_SIZE) != 1) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
    return iv;
}

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::EncryptData(
    const std::vector<std::uint8_t>& data,
    const std::vector<std::uint8_t>& key,
    const std::vector<std::uint8_t>& iv,
    std::vector<std::uint8_t>& tag,
    const std::vector<std::uint8_t>& aad) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    std::vector<std::uint8_t> ciphertext(data.size() + EVP_MAX_BLOCK_LENGTH);
    int len;
    int ciphertext_len;

    // 初始化加密上下文
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 设置 IV 长度
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr)
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 设置密钥和 IV
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 处理附加认证数据
    if (!aad.empty()) {
        if (EVP_EncryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
        }
    }

    // 加密数据
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, data.data(), static_cast<int>(data.size()))
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
    ciphertext_len = len;

    // 结束加密
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
    ciphertext_len += len;

    // 获取认证标签
    tag.resize(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_SIZE), tag.data())
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::DecryptData(
    const std::vector<std::uint8_t>& encrypted_data,
    const std::vector<std::uint8_t>& key,
    const std::vector<std::uint8_t>& iv,
    const std::vector<std::uint8_t>& tag,
    const std::vector<std::uint8_t>& aad) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    std::vector<std::uint8_t> plaintext(encrypted_data.size());
    int len;
    int plaintext_len;
    int ret;

    // 初始化解密上下文
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 设置 IV 长度
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr)
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 设置密钥和 IV
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 处理附加认证数据
    if (!aad.empty()) {
        if (EVP_DecryptUpdate(ctx, nullptr, &len, aad.data(), static_cast<int>(aad.size())) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
        }
    }

    // 解密数据
    if (EVP_DecryptUpdate(ctx,
                          plaintext.data(),
                          &len,
                          encrypted_data.data(),
                          static_cast<int>(encrypted_data.size()))
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
    plaintext_len = len;

    // 设置认证标签
    if (EVP_CIPHER_CTX_ctrl(ctx,
                            EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(tag.size()),
                            const_cast<std::uint8_t*>(tag.data()))
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    // 结束解密
    ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret > 0) {
        plaintext_len += len;
        plaintext.resize(plaintext_len);
        return plaintext;
    } else {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
}

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::EncryptKey(
    const std::vector<std::uint8_t>& key, const std::string& public_key_pem) {
    BIO* bio = BIO_new_mem_buf(public_key_pem.data(), -1);
    if (!bio) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    RSA* rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!rsa) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    int rsa_size = RSA_size(rsa);
    if (key.size() > static_cast<size_t>(rsa_size - 42)) {
        RSA_free(rsa);
        return std::unexpected("Input key size exceeds the limit for RSA-OAEP encryption");
    }
    std::vector<std::uint8_t> encrypted_key(rsa_size);
    int result = RSA_public_encrypt(static_cast<int>(key.size()),
                                    key.data(),
                                    encrypted_key.data(),
                                    rsa,
                                    RSA_PKCS1_OAEP_PADDING);
    RSA_free(rsa);

    if (result == -1) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    encrypted_key.resize(result);
    return encrypted_key;
}

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::DecryptKey(
    const std::vector<std::uint8_t>& encrypted_key, const std::string& private_key_pem) {
    BIO* bio = BIO_new_mem_buf(private_key_pem.data(), -1);
    if (!bio) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    RSA* rsa = PEM_read_bio_RSAPrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!rsa) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    int rsa_size = RSA_size(rsa);
    std::vector<std::uint8_t> key(rsa_size);
    int result = RSA_private_decrypt(static_cast<int>(encrypted_key.size()),
                                     encrypted_key.data(),
                                     key.data(),
                                     rsa,
                                     RSA_PKCS1_OAEP_PADDING);
    RSA_free(rsa);

    if (result == -1) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    key.resize(result);
    return key;
}

std::expected<std::vector<std::uint8_t>, ErrorType> FileEncryptor::EncryptFile(
    const std::string& input_file,
    const std::string& output_file,
    const std::vector<std::uint8_t>& key,
    const std::vector<std::uint8_t>& iv) {
    std::ifstream in(input_file, std::ios::binary);
    if (!in) {
        return std::unexpected("Failed to open input file");
    }

    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        return std::unexpected("Failed to open output file");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr)
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    std::vector<std::uint8_t> buffer(4096);
    std::vector<std::uint8_t> ciphertext(4096 + EVP_MAX_BLOCK_LENGTH);
    int len;
    std::vector<std::uint8_t> tag;

    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        std::streamsize count = in.gcount();

        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, buffer.data(), static_cast<int>(count))
            != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
        }

        out.write(reinterpret_cast<char*>(ciphertext.data()), len);
    }

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data(), &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }
    out.write(reinterpret_cast<char*>(ciphertext.data()), len);

    tag.resize(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(TAG_SIZE), tag.data())
        != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected(ERR_error_string(ERR_get_error(), nullptr));
    }

    EVP_CIPHER_CTX_free(ctx);
    return tag;
}

bool FileEncryptor::DecryptFile(const std::string& input_file,
                                const std::string& output_file,
                                const std::vector<std::uint8_t>& key,
                                const std::vector<std::uint8_t>& iv,
                                const std::vector<std::uint8_t>& tag) {
    std::ifstream in(input_file, std::ios::binary);
    if (!in) {
        spdlog::error("Failed to open input file");
        return false;
    }

    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        spdlog::error("Failed to open output file");
        return false;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
        return false;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr)
        != 1) {
        spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    std::vector<std::uint8_t> buffer(4096);
    std::vector<std::uint8_t> plaintext(4096);
    int len;
    int ret;

    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        std::streamsize count = in.gcount();

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, buffer.data(), static_cast<int>(count))
            != 1) {
            spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
            EVP_CIPHER_CTX_free(ctx);
            return false;
        }

        out.write(reinterpret_cast<char*>(plaintext.data()), len);
    }

    if (EVP_CIPHER_CTX_ctrl(ctx,
                            EVP_CTRL_GCM_SET_TAG,
                            static_cast<int>(tag.size()),
                            const_cast<std::uint8_t*>(tag.data()))
        != 1) {
        spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
        EVP_CIPHER_CTX_free(ctx);
        return false;
    }

    ret = EVP_DecryptFinal_ex(ctx, plaintext.data(), &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret > 0) {
        out.write(reinterpret_cast<char*>(plaintext.data()), len);
        return true;
    } else {
        spdlog::error("{}", ERR_error_string(ERR_get_error(), nullptr));
        return false;
    }
}

void FileEncryptor::secureZeroMemory(void* data_ptr, std::size_t len) {
    if (data_ptr && len > 0) {
        OPENSSL_cleanse(data_ptr, len);
    }
}

FileEncryptor::FileEncryptor() {
    OpenSSLProvider::InitOpenSSL();
}

} // namespace lansend::core