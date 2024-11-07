package main

import (
	"fmt"
	"net"
	"time"

	"github.com/google/go-cmp/cmp"
)

type LoginHandler func(host *Host, client *net.UDPAddr)
type LogoutHandler func(host *Host, client *net.UDPAddr)
type MessageHandler func(host *Host, client *net.UDPAddr, data []byte)

type Host struct {
	Conn                *net.UDPConn
	Addr                net.UDPAddr
	Clients             []*net.UDPAddr
	loginEventHandler   []LoginHandler
	logoutEventHandler  []LogoutHandler
	messageEventHandler []MessageHandler
}

func Bootstrap(ip string, port int) (*Host, error) {
	var err error = nil
	addr := net.UDPAddr{
		Port: port,
		IP:   net.ParseIP(ip),
	}
	conn, err := net.ListenUDP("udp", &addr)
	if err != nil {
		return &Host{}, err
	}
	c := make([]*net.UDPAddr, 0)
	return &Host{
		Conn:    conn,
		Addr:    addr,
		Clients: c,
	}, err
}

func (h *Host) Listen() {
	buffer := make([]byte, 1024)
	for {
		h.Conn.SetReadDeadline(time.Now().Add(1 * time.Second))
		n, client, err := h.Conn.ReadFromUDP(buffer)
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				// Timeout
				continue
			}
			fmt.Println("Erro trying to read data:", err)
			continue
		}
		str_data := string(buffer[:n])
		switch str_data {
		case "login":
			login(h, client)
			continue
		case "logout":
			logout(h, client)
			continue
		}
		message(h, client, buffer[:n])
	}
}

func login(host *Host, client *net.UDPAddr) {
	host.Clients = append(host.Clients, client)
	for _, handler := range host.loginEventHandler {
		handler(host, client)
	}
}

func logout(host *Host, client *net.UDPAddr) {
	for i, c := range host.Clients {
		if len(host.Clients) == 1 {
			host.Clients = host.Clients[:0]
			break
		}
		if cmp.Equal(*c, *client) {
			host.Clients = append(host.Clients[:i], host.Clients[i+1:]...)
		}
	}
	for _, handler := range host.logoutEventHandler {
		handler(host, client)
	}
}

func message(host *Host, client *net.UDPAddr, data []byte) {
	for _, handler := range host.messageEventHandler {
		handler(host, client, data)
	}
}

func (h *Host) AddLoginHandler(handler LoginHandler) {
	h.loginEventHandler = append(h.loginEventHandler, handler)
}

func (h *Host) AddLogoutHandler(handler LogoutHandler) {
	h.logoutEventHandler = append(h.logoutEventHandler, handler)
}

func (h *Host) AddMessageHandler(handler MessageHandler) {
	h.messageEventHandler = append(h.messageEventHandler, handler)
}
