#pragma once

#ifdef ENABLE_SSL
#include <asio/ssl.hpp>
#endif

using namespace asio::ssl;

namespace ip{
#ifdef ENABLE_SSL
    inline void use_private_key_data(context *ctx, const std::string &key_data, const context::file_format format, asio::error_code &ec) {
        const asio::const_buffer buffer(key_data.data(), key_data.size());
        ctx->use_private_key(buffer, format, ec);
    }

    inline void use_private_key_file(context *ctx, const std::string &filename, const context::file_format format, asio::error_code &ec) {
        ctx->use_private_key_file(filename, format, ec);
    }

    inline void use_certificate_data(context *ctx, const std::string &cert_data, const context::file_format format, asio::error_code &ec) {
        const asio::const_buffer buffer(cert_data.data(), cert_data.size());
        ctx->use_certificate(buffer, format, ec);
    }

    inline void use_certificate_file(context *ctx, const std::string &filename, const context::file_format format, asio::error_code &ec) {
        ctx->use_certificate_file(filename, format, ec);
    }

    inline void use_certificate_chain_data(context *ctx, const std::string &cert, asio::error_code &ec) {
        const asio::const_buffer buffer(cert.data(), cert.size());
        ctx->use_certificate_chain(buffer, ec);
    }

    inline void use_certificate_chain_file(context *ctx, const std::string &filename, asio::error_code &ec) {
        ctx->use_certificate_chain_file(filename, ec);
    }

    inline void load_verify_file(context *ctx, const std::string &filename, asio::error_code &ec) {
        ctx->load_verify_file(filename, ec);
    }
#endif
}