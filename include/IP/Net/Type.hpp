/*
 * Copyright (c) 2023-2025 Nathan Miguel
 *
 * InternetProtocol is free library: you can redistribute it and/or modify it under the terms
 * of the GNU Affero General Public License as published by the Free Software Foundation,
 * version 3.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero General Public License for more details.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
*/

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
