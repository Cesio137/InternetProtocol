import * as net from "net";

const PORT = 3000;
const HOST = "localhost";

const server = net.createServer(function(socket) {
    console.log('Client connected:', socket.remoteAddress, socket.remotePort);

    socket.on('data', (data:Buffer) => {
        console.log('Received from client:', data.toString());
        socket.write(data);
    });

    socket.on('end', () => {
        console.log('Client disconnected: ', socket.remoteAddress, socket.remotePort);
    });

    socket.on('close', function(hadError:boolean) {
        console.log('Client disconnected: ', socket.remoteAddress, socket.remotePort);
    });

    socket.on('error', (err:Error) => {
        console.error('Socket error:', err);
    });
});

server.listen(PORT, HOST, () => {
    console.log(`Server listening on ${HOST}:${PORT}`);
});

server.on('error', (err:Error) => {
    console.error('Server error:', err);
});

