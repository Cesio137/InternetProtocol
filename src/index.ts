import { udp, tcp, http, ws } from "#resolver";

const port = 3000;
const args = process.argv;
const resolver_arg = args.length >= 0 ? args[2] : "udp";
const use_openssl = args[3] == "openssl" ? true : false;
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

switch (resolver_arg) {
    case "udp_server":
        udp.resolve(port);
        break;

    case "udp_client":
        udp.connect(port);
        break;

    case "tcp_server":
        if (use_openssl) tcp.ssl_resolve(port);
        else tcp.resolve(port);
        break;

    case "tcp_client":
        if (use_openssl) tcp.ssl_connect(port);
        else tcp.connect(port);
        break;

    case "http_server":
        if (use_openssl) http.ssl_resolve(port);
        else http.resolve(port);
        break;

    case "http_client":
        if (use_openssl) http.ssl_connect(port);
        else http.connect(port);
        break;

    case "ws_server":
        if (use_openssl) ws.ssl_resolve(port);
        else ws.resolve(port);
        break;

    case "ws_client":
        if (use_openssl) ws.ssl_connect(port);
        else ws.connect(port);
        break;

    default:
        udp.resolve(port);
        break;
}
