#pragma once

#include <string>

struct SecurityContext {
    std::string private_key_pem;
    std::string public_key_pem;
    std::string certificate_pem;
    std::string certificate_hash; // SHA-256 fingerprint
};