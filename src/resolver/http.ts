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
    const JsonMessage: Handle = {event: 'http_request', message: 'Received request from nodejs!'};
    res.send(JSON.stringify(JsonMessage));
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


