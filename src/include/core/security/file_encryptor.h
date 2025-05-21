#pragma once


#include <expected>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <string>
#include <vector>

namespace lansend {


/**
 * @class FileEncryptor
 * @brief 提供文件加密和解密功能的类
 * 
 * 使用AES-256-GCM进行加密和解密，提供身份验证和保密性。
 * 使用RSA进行密钥加密，确保密钥传输的安全性。
 */
class FileEncryptor {
    using ErrorType = std::string;

public:
    // AES-GCM密钥大小（256位）
    static constexpr size_t KEY_SIZE = 32;
    // AES-GCM IV大小（12字节）
    static constexpr size_t IV_SIZE = 12;
    // AES-GCM认证标签大小（16字节）
    static constexpr size_t TAG_SIZE = 16;

    /**
     * @brief 生成随机AES密钥
     * @param key_size 密钥大小（默认为256位/32字节）
     * @return 生成的随机密钥
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> GenerateKey(int key_size = KEY_SIZE);

    /**
     * @brief 生成随机初始化向量(IV)
     * @return 生成的随机IV
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> GenerateIV();

    /**
     * @brief 加密数据块
     * @param data 要加密的数据
     * @param key 加密密钥
     * @param iv 初始化向量
     * @param tag 输出参数，认证标签
     * @param aad 附加验证数据（可选）
     * @return 加密后的数据
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> EncryptData(
        const std::vector<std::uint8_t>& data,
        const std::vector<std::uint8_t>& key,
        const std::vector<std::uint8_t>& iv,
        std::vector<std::uint8_t>& tag,
        const std::vector<std::uint8_t>& aad = {});

    /**
     * @brief 解密数据块
     * @param encrypted_data 加密的数据
     * @param key 解密密钥
     * @param iv 初始化向量
     * @param tag 认证标签
     * @param aad 附加验证数据（可选）
     * @return 解密后的数据
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> DecryptData(
        const std::vector<std::uint8_t>& encrypted_data,
        const std::vector<std::uint8_t>& key,
        const std::vector<std::uint8_t>& iv,
        const std::vector<std::uint8_t>& tag,
        const std::vector<std::uint8_t>& aad = {});

    /**
     * @brief 使用RSA公钥加密AES密钥
     * @param key 要加密的AES密钥
     * @param public_key_pem PEM格式的RSA公钥
     * @return 加密后的密钥
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> EncryptKey(
        const std::vector<std::uint8_t>& key, const std::string& public_key_pem);

    /**
     * @brief 使用RSA私钥解密AES密钥
     * @param encrypted_key 加密的AES密钥
     * @param private_key_pem PEM格式的RSA私钥
     * @return 解密后的密钥
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> DecryptKey(
        const std::vector<std::uint8_t>& encrypted_key, const std::string& private_key_pem);

    /**
     * @brief 加密文件
     * @param input_file 输入文件路径
     * @param output_file 输出文件路径
     * @param key 加密密钥
     * @param iv 初始化向量
     * @return 认证标签
     */
    static std::expected<std::vector<std::uint8_t>, ErrorType> EncryptFileA(
        const std::string& input_file,
        const std::string& output_file,
        const std::vector<std::uint8_t>& key,
        const std::vector<std::uint8_t>& iv);

    /**
     * @brief 解密文件
     * @param input_file 加密的文件路径
     * @param output_file 输出文件路径
     * @param key 解密密钥
     * @param iv 初始化向量
     * @param tag 认证标签
     * @return 解密是否成功
     */
    static bool DecryptFileA(const std::string& input_file,
                            const std::string& output_file,
                            const std::vector<std::uint8_t>& key,
                            const std::vector<std::uint8_t>& iv,
                            const std::vector<std::uint8_t>& tag);

private:
    // 安全清除内存
    static void secureZeroMemory(void* data_ptr, std::size_t len);

};

} // namespace lansend

