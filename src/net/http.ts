import express, { type Request, type Response } from "express";
import { Axios } from "axios";
import https from "https";
import { Server, IncomingMessage, ServerResponse } from "http";
import { credentials } from "#settings";
import { rl } from "#io";

export class httpserver {
    constructor() {
        this.app.get("/", this.get);
    }

    public listen(port: number) {
        this.server = this.app.listen(port, (err) => {
            if (err) {
                this.onerror(err);
                return;
            }
            this.onlistening();
        });

        this.server.on("close", this.onclose);
        this.server.on("error", this.onerror);
    }

    public close() {
        if (this.server) {
            this.server.closeAllConnections();
            this.server.close();
        }
        process.exit();
    }

    private app = express();
    private server: Server<typeof IncomingMessage, typeof ServerResponse> | undefined;

    private get(req: Request, res: Response) {
        const connectionHeader = req.headers["connection"];
        console.log(`Connection header value: ${connectionHeader}`);
        const response = `Your remote port is: ${req.socket.remotePort}`;
        res.send(response);
    }

    private online(input: string) {
        if (input === "quit") {
            rl.close();
            this.close();
            return;
        }
    }

    private onlistening() {
        console.log(`HTTP server listening on ${this.server?.address()?.toString()}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private onclose() {
        console.log("bye!");
    }

    private onerror(err: Error) {
        console.error(err);
    }
}

 export class httpserver_ssl {
    constructor() {
        this.app.get("/", this.get);
    }

    public listen(port: number) {
        this.server = this.sslServer.listen(port, () => {
            this.onlistening();
        });

        this.server.on("close", this.onclose);
        this.server.on("error", this.onerror);
    }

    public close() {
        if (this.server) {
            this.server.closeAllConnections();
            this.server.close();
        }
        process.exit();
    }

    private app = express();
    private sslServer = https.createServer(credentials, this.app);
    private server: https.Server<typeof IncomingMessage, typeof ServerResponse> | undefined;

    private get(req: Request, res: Response) {
        const connectionHeader = req.headers["connection"];
        console.log(`Connection header value: ${connectionHeader}`);
        const response = `Your remote port is: ${req.socket.remotePort}`;
        res.send(response);
    }

    private online(input: string) {
        if (input === "quit") {
            rl.close();
            this.close();
            return;
        }
    }

    private onlistening() {
        console.log(`HTTP server listening on ${this.server?.address()?.toString()}`);

        rl.on("SIGINT", this.close);
        rl.on("line", this.online);
    }

    private onclose() {
        console.log("bye!");
    }

    private onerror(err: Error) {
        console.error(err);
    }
}

export class httpclient {
    constructor() {

    }

    public connect(port: number) {
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
            baseURL: `http://localhost:${port}`,
        })
        .then((response) => {
            console.log("Server response:", response.data);
        })
        .catch((error) => {
            console.error("Error:", error.message);
        });
    }

    private axios: Axios | undefined;
}

export class httpclient_ssl {
    constructor() {

    }

    public connect(port: number) {
        this.axios = new Axios({
            httpAgent: this.agent,
            method: "get",
            baseURL: `http://localhost:${port}/`,
            headers: {
                "User-Agent": "Axio",
                Connection: "close",
            },
        });

        console.log(`HTTPS client connected at https://localhost:${port}/`);

        const response = this.axios
        .get("/", {
            baseURL: `http://localhost:${port}`,
        })
        .then((response) => {
            console.log("Server response:", response.data);
        })
        .catch((error) => {
            console.error("Error:", error.message);
        });
    }

    private agent = new https.Agent(credentials);
    private axios: Axios | undefined;
}