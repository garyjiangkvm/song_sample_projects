package main

import (
	"encoding/hex"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	sockln, err := net.Listen("unix", "/var/run/storageos/dataplane-notifications.sock")
	if err != nil {
		fmt.Println(err)
		return
	}

	tcpln, err := net.Listen("tcp", ":1337")
	if err != nil {
		fmt.Println(err)
		return
	}

	go sockListenAccept(sockln)
	go tcpListenAccept(tcpln)

	// Wait for signal
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, os.Interrupt, syscall.SIGINT, os.Kill, syscall.SIGTERM)
	s := <-sigs
	log.Println("Receive signal: ", s)

	// Close HTTP Server
	err = sockln.Close()
	if err != nil {
		log.Fatalln("Sock Server Close Error - ", err)
	}

	err = tcpln.Close()
	if err != nil {
		log.Fatalln("Sock Server Close Error - ", err)
	}
	log.Println("Server Closed")
}

func sockListenAccept(ln net.Listener) {
	for {
		c, err := ln.Accept()
		if err != nil {
			fmt.Println(err)
			continue
		}
		// handle the connection
		go handleSockConnection(c.(*net.UnixConn))
	}

}

func handleSockConnection(c *net.UnixConn) {
	// receive the message
	buff := make([]byte, 0x100)
	fmt.Println("server connection")

	count, err := c.Read(buff)
	if err != nil {
		fmt.Println(err)

	}
	fmt.Printf("Tcp read count %d ---\n %s\n", count, buff)
	log.Printf("-------REQ-------\n%s\n                    ------------------\n", hex.Dump(buff))

	f, err := os.Create("./byte")
	f.Write(buff)
	f.Close()
}

func tcpListenAccept(ln net.Listener) {
	for {
		c, err := ln.Accept()
		if err != nil {
			fmt.Println(err)
			continue
		}
		// handle the connection
		go handleTcpConnection(c.(*net.TCPConn))
	}

}

func handleTcpConnection(c *net.TCPConn) {
	// receive the message
	buff := make([]byte, 0x100)
	fmt.Println("server connection")

	count, err := c.Read(buff)
	if err != nil {
		fmt.Println(err)

	}
	fmt.Printf("Tcp read count %d ---\n %s\n", count, buff)
	log.Printf("-------REQ-------\n%s\n                    ------------------\n", hex.Dump(buff))
}
