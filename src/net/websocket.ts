import { type AddressInfo, type RawData, WebSocket, WebSocketServer } from "ws";
import https from "https";
import { IncomingMessage } from "http";
import { credentials } from "#settings";
import { rl } from "#io";

export class websocketserver {
    constructor() {}

    write(message: string) {
        if (typeof this.server === "undefined") return;
        for (const client of this.server.clients) {
            client.send(message, (err) => {
                if (err)
                    console.error(err);
            });
        }
    }

    listen(port: number) {
        this.server = new WebSocketServer({ port });
        this.server.on("listening", () => this.onlistening());
        this.server.on("connection", (ws, req) => this.onconnection(ws, req));
        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));
    }

    close() {
        if (typeof this.server === "undefined") return;
        for (const client of this.server.clients) {
            client.close();
        }
        this.server.close();
        process.exit();
    }

    server?: WebSocketServer;

    online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            this.close();
            return;
        }
        this.write(input);
    }

    onlistening() {
        if (typeof this.server === "undefined") return;
        const addr = this.server.address();
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`WS listenings on localhost:${(addr as AddressInfo).port}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    onconnection(ws: WebSocket, req: IncomingMessage) {
        if (typeof this.server === "undefined") return;
        console.log(
                `(${req.socket.remotePort} -> login, ${this.server.clients.size} client(s))`
            );

        ws.on("message", (data, isBinary) => this.onclientmessage(data, isBinary, req));
        ws.on("close", () => this.onclientclose(req));
        ws.on("error", (err) => this.onclienterror(err));
    }

    onclose() {
        console.log("bye!");
    }

    onerror(err: Error) {
        console.error(err);
    }

    // Clients

    onclientmessage(data: WebSocket.RawData, isBinary: boolean, req: IncomingMessage) {
        const message = `${req.socket.remotePort} -> ${data.toString()}`;
        console.log(message);
    }

    onclientclose(req: IncomingMessage) {
        console.log(
                `(${req.socket.remotePort} -> logout, ${this.server?.clients.size} client(s))`
            );
    }

    onclienterror(err: Error) {
        console.error(err);
    }
}

export class websocketserver_ssl extends websocketserver {
    httpsServer = https.createServer(credentials);

    override listen(port: number) {
        this.server = new WebSocketServer({ server: this.httpsServer });
        this.httpsServer.listen(port, () => {
            console.log(`WSS listening at adress localhost:${port}`);
        });
        this.server.on("listening", () => this.onlistening());
        this.server.on("connection", (ws, req) => this.onconnection(ws, req));
        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));
    }

    override close() {
        if (typeof this.server === "undefined") return;
        this.httpsServer.close();
        for (const client of this.server.clients) {
            client.close();
        }
        this.server.close();
    }

    override onlistening() {
        rl.on("SIGINT", () => this.close());
        rl.on("line", (err) => this.online(err));
    }
}

export class websocketclient {
    constructor() {}

    client?: WebSocket;

    write(menssage: string) {
        this.client?.send(menssage, (err) => {
            if (err)
                console.error(err);
        });
    }

    connect(port: number) {
        this.client = new WebSocket(`ws:localhost:${port}`);
        this.client?.on("open", () => this.onopen());
        this.client?.on("message", (data, isBinary) => this.onmessage(data, isBinary));
        this.client?.on("close", () => this.onclose());
        this.client?.on("error", (err) => this.onerror(err));
    }

    close() {
        this.client?.close();
        process.exit();
    }

    online(input: string) {
        if (input === "") return;
        if (input === "quit") {
            this.close();
            return;
        }
        this.write(input);
    }

    onopen() {
        console.log(`WS connected at adress: ${this.client?.url}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }

    onmessage(data: WebSocket.RawData, isBinary: boolean) {
        const message = data.toString();
        console.log(message);
    }

    onclose() {
        console.log("bye!");
    }

    onerror(err: Error) {
        console.error(err);
    }
}

export class websocketclient_ssl extends websocketclient {
    httpAgent = new https.Agent(credentials);

    override connect(port: number) {
        this.client = new WebSocket(`wss:localhost:${port}`, { agent:  this.httpAgent});
        this.client?.on("open", () => this.onopen());
        this.client?.on("message", (ws, req) => this.onmessage(ws, req));
        this.client?.on("close", () => this.onclose());
        this.client?.on("error", (err) => this.onerror(err));
    }

    override onopen() {
        console.log(`WSS connected at adress: ${this.client?.url}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }
}