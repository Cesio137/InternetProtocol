import { udp, tcp, http, ws } from "#resolver";

const port = 3000;
const args = process.argv;
const resolver_arg = args.length >= 0 ? args[2] : "udp";
const use_openssl = args[3] == "openssl" ? true : false;
console.log(resolver_arg)
switch (resolver_arg) {
    case "udp":
        udp.resolve(port);
        break;

    case "tcp":
        if (use_openssl) tcp.ssl_resolve(port);
        else tcp.resolve(port);
        break;

    case "http":
        if (use_openssl) http.ssl_resolve(port);
        else http.resolve(port);
        break;

    case "ws":
        if (use_openssl) ws.ssl_resolve(port);
        else ws.resolve(port);
        break;

    default:
        udp.resolve(port);
        break;
}
