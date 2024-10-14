import * as tls from "tls";
import * as fs from "fs";

const options = {
    key: fs.readFileSync("server-key.pem"),
    cert: fs.readFileSync("server-cert.pem"),
    ca: fs.readFileSync("ca-cert.pem"),
};

const PORT = 3000;
const HOST = "localhost";

const server = tls.createServer(options, (socket) => {
    console.log('Client connected:', socket.remoteAddress, socket.remotePort);

    socket.on("data", (data) => {
        console.log("Received from client:", data.toString());
        socket.write(data);
    });

    socket.on("end", () => {
        console.log("Client disconnected: ", socket.remoteAddress, socket.remotePort);
    });

    socket.on("close", function (hadError: boolean) {
        console.log("Client disconnected: ", socket.remoteAddress, socket.remotePort);
    });

    socket.on("error", (err: Error) => {
        console.error("Socket error:", err);
    });
});

server.listen(PORT, HOST, () => {
    console.log(`Server listening on ${HOST}:${PORT}`);
});

server.on("error", (err: Error) => {
    console.error("Server error:", err);
});
