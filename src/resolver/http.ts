import express, { Request, Response } from "express";
import { Axios } from "axios";
import https from "https";
import { credentials } from "#settings";
import { rl } from "#io";

type Handle = {
    event: string;
    message: string;
};

const app = express();
const ssl_server = https.createServer(credentials, app);

app.get("/", (req: Request, res: Response) => {
    const connectionHeader = req.headers["connection"];
    console.log(`Valor do cabeçalho Connection: ${connectionHeader}`);
    const response = `Your remote port is: ${req.socket.remotePort}`;
    res.send(response);
});

export function resolve(port: number) {
    const server = app.listen(port, function () {
        console.log(`HTTP listening at adress localhost:${port}`);
        rl.on("SIGINT", function () {
            server.close();
            console.log("bye!");
            process.exit();
        });
    });
}

export function ssl_resolve(port: number) {
    ssl_server.listen(port, () => {
        console.log(`HTTPS listening at adress localhost:${port}`);
    });
}

const client = new Axios({
    method: "get",
    baseURL: "http://localhost:3000/",
    headers: {
        "User-Agent": "Axio",
        Connection: "close",
    },
});

const agent = new https.Agent(credentials);
const ssl_client = new Axios({
    httpsAgent: agent,
    baseURL: "https://localhost:3000/",
    method: "get",
    headers: {
        "User-Agent": "Axio",
        Connection: "close",
    },
});

export function connect(port: number) {
    const response = client
        .get("/", {
            baseURL: `http://localhost:${port}`,
        })
        .then((response) => {
            console.log("Resposta do servidor:", response.data);
        })
        .catch((error) => {
            console.error("Erro ao conectar:", error.message);
        });
}

export function ssl_connect(port: number) {
    const response = ssl_client
        .get("/", {
            baseURL: `https://127.0.0.1:${port}`,
        })
        .then((response) => {
            console.log("Resposta do servidor:", response.data);
        })
        .catch((error) => {
            console.error("Erro ao conectar:", error.message);
        });
}
