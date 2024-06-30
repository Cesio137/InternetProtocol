import { Address } from "cluster";
import * as dgram from "dgram";
import { AddressInfo } from "net";

const HOST: string = "localhost";
const PORT: number = 3000;

const server: dgram.Socket = dgram.createSocket("udp4");

server.on("message", (msg: Buffer, rinfo: dgram.RemoteInfo) => {
  console.log(`Client: ${rinfo.address}:${rinfo.port}`);
  console.log(`Message: ${msg}`);

  const response: string = `Server: ${rinfo.address}:${
    rinfo.port
  } \nMessage: ${msg.toString()}`;
  server.send(response, rinfo.port, rinfo.address, (error) => {
    if (error) {
      console.error(`Erro trying send message: ${error.message}`);
      return;
    }
    console.log(`Message sent to ${rinfo.address}:${rinfo.port}`);
  });
});

server.on("listening", () => {
  const address: AddressInfo = server.address();
  console.log(`Listening at adress: ${address.address}:${address.port}`);
});

server.on("error", (error: Error) => {
  console.error(`Server error: ${error.message}`);
  server.close();
});

server.bind(PORT, HOST);
