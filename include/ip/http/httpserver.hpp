#pragma once

#include "ip/net/common.hpp"
#include "ip/tcp/tcpserver.hpp"

namespace ip {
    class http_server_c : private tcp_server_c {
    public:


        bool open(const server_bind_options_t &bind_opts) override {
            return tcp_server_c::open(bind_opts);
        }

    private:

    };
}
