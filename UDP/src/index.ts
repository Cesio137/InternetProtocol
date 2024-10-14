import { Address } from "cluster";
import * as dgram from "dgram";
import { AddressInfo } from "net";

const HOST: string = "localhost";
const PORT: number = 3000;

const server: dgram.Socket = dgram.createSocket("udp4");

server.on("message", function(msg: Buffer, rinfo: dgram.RemoteInfo) {
  console.log(`Client: ${rinfo.address}:${rinfo.port}`);
  console.log(`Message: ${msg}`);

  const response: Buffer = msg;
  server.send(response, rinfo.port, rinfo.address, function(error) {
    if (error) {
      console.error(`Erro trying send message: ${error.message}`);
      return;
    }
    console.log(`Message sent to ${rinfo.address}:${rinfo.port}`);
  });
});

server.on("listening", function() {
  const address: AddressInfo = server.address();
  console.log(`Listening at adress: ${address.address}:${address.port}`);
});

server.on("error", function(error: Error) {
  console.error(`Server error: ${error.message}`);
  server.close();
});

server.bind(PORT, HOST);
