import express, { Express, Request, Response } from 'express'
import https from "https";
import { credentials } from '#settings';

type Handle = {
    event:string,
    message:string
}

const app: Express = express();
const https_server = https.createServer(credentials, app);

app.get('/', (req:Request, res:Response) => {
    const response = `Your remote port is: ${req.socket.remotePort}`
    res.send(response);
});

export function resolve(port: number) {
    app.listen(port, function(){
        console.log(`HTTP listening at adress localhost:${port}`);
    });
}

export function ssl_resolve(port: number) {
    https_server.listen(port, () => {
        console.log(`HTTP SSL listening at adress localhost:${port}`);
    });
}


