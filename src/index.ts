import { udpserver, udpclient, tcpserver, tcpserver_ssl, tcpclient, tcpclient_ssl, httpserver, httpserver_ssl, httpclient, httpclient_ssl } from "#net";

const port = 3000;
const args = process.argv;
const resolver_arg = args.length >= 0 ? args[2] : "udp";
const use_openssl = args[3] == "openssl" ? true : false;
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

switch (resolver_arg) {
    case "udp-server":
        const udpServer = new udpserver()
        udpServer.open(port);
        break;

    case "udp-client":
        const udpClient = new udpclient();
        udpClient.connect(port);
        break;

    case "tcp-server":
        if (use_openssl) {
            const tcpServer = new tcpserver_ssl()
            tcpServer.listen(port);
        } else {
            const tcpServer = new tcpserver()
            tcpServer.listen(port);
        }
        break;

    case "tcp-client":
        if (use_openssl) {
            const tcpClient = new tcpclient_ssl()
            tcpClient.connect(port);
        } else {
            const tcpClient = new tcpclient()
            tcpClient.connnect(port);
        }
        break;

    case "http-server":
        if (use_openssl) {
            const httpClient = new httpserver_ssl()
            httpClient.listen(port);
        } else {
            const httpClient = new httpserver()
            httpClient.listen(port);
        }
        break;

    case "http-client":
        if (use_openssl) {
            const httpClient = new httpclient_ssl()
            httpClient.connect(port);
        } else {
            const httpClient = new httpclient()
            httpClient.connect(port);
        }
        break;

    default:
        const net = new udpserver();
        net.open(port);
        break;
}