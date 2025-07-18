import express, { type Request, type Response } from "express";
import { Axios } from "axios";
import https from "https";
import { Server, IncomingMessage, ServerResponse } from "http";
import { credentials } from "#settings";
import { rl } from "#io";
import type { AddressInfo } from "net";

export class httpserver {
    constructor() {
        this.app.get("/", (req, res) => this.get(req, res));
    }

    listen(port: number) {
        this.server = this.app.listen(port, (err) => {
            if (err) {
                this.onerror(err);
                return;
            }
            this.onlistening();
        });

        this.server.on("close", () => this.onclose());
        this.server.on("error", (err) => this.onerror(err));
    }

    close() {
        if (this.server) {
            this.server.closeAllConnections();
            this.server.close();
        }
        process.exit();
    }

    app = express();
    server: Server<typeof IncomingMessage, typeof ServerResponse> | undefined;

    get(req: Request, res: Response) {
        const connectionHeader = req.headers["connection"];
        console.log(`Connection header value: ${connectionHeader}`);
        const response = `Your remote port is: ${req.socket.remotePort}`;
        res.send(response);
    }

    online(input: string) {
        if (input === "quit") {
            rl.close();
            this.close();
            return;
        }
    }

    onlistening() {
        const addr = this.server?.address();
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`HTTP listenings on localhost:${(addr as AddressInfo).port}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    onclose() {
        console.log("bye!");
    }

    onerror(err: Error) {
        console.error(err);
    }
}

 export class httpserver_ssl extends httpserver {
    sslServer: https.Server;

    constructor() {
        super();
        this.sslServer = https.createServer(credentials, this.app);
    }

    override listen(port: number) {
        this.sslServer.listen(port, () => this.onlistening());
        this.sslServer.on("close", () => this.onclose());
        this.sslServer.on("error", (err) => this.onerror(err));
    }

    override close() {
        if (this.sslServer) {
            this.sslServer.close();
        }
        process.exit();
    }

    override onlistening() {
        const addr = this.sslServer.address();
        if (addr && typeof addr === "object" && "address" in addr)
            console.log(`HTTPS listenings on localhost:${(addr as AddressInfo).port}`);

        rl.on("SIGINT", () => this.close());
        rl.on("line", (input) => this.online(input));
    }
}

export class httpclient {
    constructor() {}

    axios: Axios | undefined;

    connect(port: number) {
        this.axios = new Axios({
            method: "get",
            baseURL: `http://localhost:${port}/`,
            headers: {
                "User-Agent": "Axio",
                Connection: "close",
            },
        });

        console.log(`HTTP client connected at http://localhost:${port}/`);
        const response = this.axios
        .get("/", {
            baseURL: `http://localhost:${port}/`,
        })
        .then((response) => {
            console.log("Server response:", response.data);
        })
        .catch((error) => {
            console.error("Error:", error.message);
        });
    }    
}

export class httpclient_ssl extends httpclient {
    agent = new https.Agent({ ...credentials, rejectUnauthorized: false });

    override connect(port: number) {
        this.axios = new Axios({
            httpsAgent: this.agent,
            method: "get",
            baseURL: `https://localhost:${port}/`,
            headers: {
                "User-Agent": "Axio",
                Connection: "close",
            },
        });

        console.log(`HTTPS client connected at https://localhost:${port}/`);

        this.axios
            .get("/", {
                baseURL: `https://localhost:${port}/`,
                httpsAgent: this.agent,
            })
            .then((response) => {
                console.log("Server response:", response.data);
            })
            .catch((error) => {
                console.error("Error:", error.message);
            });
    }
}