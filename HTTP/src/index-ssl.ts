import express from "express";
import https from "https";
import fs from "fs";

type Handle = {
    event:string,
    message:string
}

const key = fs.readFileSync("server-key.pem", "utf8");
const cert = fs.readFileSync("server-cert.pem", "utf8");
const credentials = { key: key, cert: cert };
const PORT = 3000;
const app = express();

app.get("/", (req, res) => {
    const JsonMessage: Handle = {event: 'http_request', message: 'Received request from nodejs!'};
    res.send(JSON.stringify(JsonMessage));
});

const httpsServer = https.createServer(credentials, app);

httpsServer.listen(PORT, () => {
    console.log(`listening at adress localhost:${PORT}`);
});
