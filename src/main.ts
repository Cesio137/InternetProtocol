import { udp, tcp, http, ws } from "#resolver";

const PORT = 3000;
const args = Deno.args;
const resolver_arg = args.length >= 0 ? args[0] : "udp";
const use_openssl = args[1] == "openssl" ? true : false;

Deno.addSignalListener("SIGINT", () => {
    console.log("Bye");
    switch (resolver_arg) {
        case "udp":
            udp.close();
            break;
        case "tcp":
            tcp.close();
            break;
        case "http":
            http.close();
            break;
        case "websocket":
            ws.close();
            break;
    }
    Deno.exit();
});

switch (resolver_arg) {
    case "udp":
        udp.resolve({ port: PORT });
        break;
    case "tcp":
        if (use_openssl) await tcp.ssl_resolve({ port: PORT });
        else await tcp.resolve({ port: PORT });
        break;
    case "http":
        if (use_openssl) http.ssl_resolve({ port: PORT });
        else http.resolve({ port: PORT });
        break;
    case "websocket":
        if (use_openssl) ws.ssl_resolve({ port: PORT });
        else ws.resolve({ port: PORT });
        break;
}
