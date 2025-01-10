#pragma once

#define ASIO_STANDALONE
#define ASIO_NO_DEPRECATED
#include <asio.hpp>
#ifdef ASIO_USE_OPENSSL
#include <asio/ssl.hpp>
#endif

namespace InternetProtocol {
    typedef std::shared_ptr<asio::ip::tcp::socket> socket_ptr;
#ifdef ASIO_USE_OPENSSL
    typedef std::shared_ptr<asio::ssl::stream<asio::ip::tcp::socket> > ssl_socket_ptr;
#endif
}
