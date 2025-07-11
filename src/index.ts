import { udpserver, udpclient, tcpserver, tcpserver_ssl, tcpclient, tcpclient_ssl, httpserver, httpserver_ssl, httpclient, httpclient_ssl, websocketserver_ssl, websocketserver, websocketclient, websocketclient_ssl } from "#net";

const port = 8080;
const args = process.argv;
const resolver_arg = args.length >= 0 ? args[2] : "udp";
const use_tls = args[3] === "tls" ? true : false;
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

switch (resolver_arg) {
    case "udp-server":
        const udpServer = new udpserver();
        udpServer.open(port);
        break;

    case "udp-client":
        const udpClient = new udpclient();
        udpClient.connect(port);
        break;

    case "tcp-server":
        if (use_tls) {
            const tcpServer = new tcpserver_ssl();
            tcpServer.listen(port);
        } else {
            const tcpServer = new tcpserver();
            tcpServer.listen(port);
        }
        break;

    case "tcp-client":
        if (use_tls) {
            const tcpClient = new tcpclient_ssl();
            tcpClient.connect(port);
        } else {
            const tcpClient = new tcpclient();
            tcpClient.connect(port);
        }
        break;

    case "http-server":
        if (use_tls) {
            const httpServer = new httpserver_ssl();
            httpServer.listen(port);
        } else {
            const httpServer = new httpserver();
            httpServer.listen(port);
        }
        break;

    case "http-client":
        if (use_tls) {
            const httpClient = new httpclient_ssl();
            httpClient.connect(port);
        } else {
            const httpClient = new httpclient();
            httpClient.connect(port);
        }
        break;

    case "ws-server":
        if (use_tls) {
            const wsServer = new websocketserver_ssl();
            wsServer.listen(port);
        } else {
            const wsServer = new websocketserver();
            wsServer.listen(port);
        }
        break;

    case "ws-client":
        if (use_tls) {
            const wsClient = new websocketclient_ssl();
            wsClient.connect(port);
        } else {
            const wsClient = new websocketclient();
            wsClient.connect(port);
        }
        break;

    default:
        const net = new udpserver();
        net.open(port);
        break;
}