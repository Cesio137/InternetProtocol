import { type AddressInfo, type RawData, WebSocket, WebSocketServer } from "ws";
import https from "https";
import { IncomingMessage } from "http";
import { credentials } from "#settings";
import { rl } from "#io";

export class websocketserver {
    constructor(server: WebSocketServer) {
        this.server = server;
        this.server.on("listening", () => this.onlistening());
        this.server.on("connection", (ws, req) => this.onconnection(ws, req));
        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));
    }

    public static create(port: number) {
        const server = new WebSocketServer({ port });
        return new websocketserver(server);
    }

    write(message: string) {
        if (typeof this.server === "undefined") return;
        for (const client of this.server.clients) {
            client.send(message, (err) => {
                if (err)
                    console.error(err);
            });
        }
    }

    close() {
        if (typeof this.server === "undefined") return;
        for (const client of this.server.clients) {
            client.close();
        }
        this.server.close();
        process.exit();
    }

    server: WebSocketServer;

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
        ws.on("close", (code, reason) => this.onclientclose(code, reason, req));
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

    onclientclose(code: number, reason: Buffer, req: IncomingMessage) {
        console.log(
                `(${req.socket.remotePort} -> logout, ${this.server?.clients.size} client(s))\n{code: ${code} -> ${reason.toString()}}`
            );
    }

    onclienterror(err: Error) {
        console.error(err);
    }
}

export class websocketserver_ssl extends websocketserver {
    public static override create(port: number) {
        const httpsServer = https.createServer(credentials);
        const server = new WebSocketServer({ server: httpsServer });
        httpsServer.listen(port, () => {
            console.log(`WSS listening at adress localhost:${port}`);
        });
        return new websocketserver_ssl(server);
    }

    override close() {
        for (const client of this.server.clients) {
            client.close(1000, "Server going to shutdown");
        }
        this.server.close();
    }

    override onlistening() {
        rl.on("SIGINT", () => this.close());
        rl.on("line", (err) => this.online(err));
    }
}

export class websocketclient {
    constructor(client: WebSocket) {
        this.client = client;
        this.client.on("open", () => this.onopen());
        this.client.on("message", (data, isBinary) => this.onmessage(data, isBinary));
        this.client.on("close", (code, reason) => this.onclose(code, reason));
        this.client.on("error", (err) => this.onerror(err));
    }

    public static create(port: number) {
        const client = new WebSocket(`ws:localhost:${port}`);
        return new websocketclient(client);
    }

    client: WebSocket;

    write(menssage: string) {
        this.client.send(menssage, (err) => {
            if (err)
                console.error(err);
        });
    }

    close() {
        this.client.close();
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

    onclose(code: number, reason: Buffer) {
        console.log(`${code} -> ${reason.toString()}\nbye!`);
    }

    onerror(err: Error) {
        console.error(err);
    }
}

export class websocketclient_ssl extends websocketclient {
    public static override create(port: number) {
        const httpAgent = new https.Agent(credentials);
        const client = new WebSocket(`wss:localhost:${port}`, { agent:  httpAgent});
        return new websocketclient_ssl(client);
    }

    override onopen() {
        console.log(`WSS connected at adress: ${this.client?.url}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }
}