import { udp, tcp, http, ws } from "#resolver"

const port = 3000;
const args = process.argv;
let resolve_arg = "";
let use_ssl = false;
args.forEach(arg => {
    switch(arg) {
        case "udp":
            if (resolve_arg === "") {
                resolve_arg = arg;
            }
            break;

        case "tcp":
            if (resolve_arg === "") {
                resolve_arg = arg;
            }
            break;

        case "http":
            if (resolve_arg === "") {
                resolve_arg = arg;
            }
            break;

        case "ws":
            if (resolve_arg === "") {
                resolve_arg = arg;
            }
            break;

        case "openssl":
            use_ssl = true;
            break;

        default:
            break;
    }

    if (resolve_arg !== "" && use_ssl) return;
})

switch (resolve_arg) {
    case "udp":
        udp.resolve(port)
        break;
    
    case "tcp":
        if (use_ssl)
            tcp.ssl_resolve(port);
        else
            tcp.resolve(port);
        break;

    case "http":
        if (use_ssl)
            http.ssl_resolve(port);
        else
            http.resolve(port);
        break;

    case "ws":
        if (use_ssl)
            ws.ssl_resolve(port);
        else
            ws.resolve(port);
        break;

    default:
        udp.resolve(port)
        break;
}
