package main

import (
	"encoding/hex"
	"fmt"
	"log"
	"net"
	"os"
)

func main() {
	buff := make([]byte, 0x100)

	//relay message to socket
	sockConn, err := net.Dial("unix", "/var/run/storageos/dataplane-notifications.sock")
	if err != nil {
		fmt.Println(err)
		return
	}

	f, err := os.Open("./byte")
	if err != nil {
		fmt.Println(err)
		return
	}

	count, err := f.Read(buff)

	count, err = sockConn.Write(buff)
	if err != nil {
		log.Fatal("write error:", err)
	}
	fmt.Printf("Socket write count %d ---\n %s\n", count, buff)
	log.Printf("-------REQ-------\n%s\n                    ------------------\n", hex.Dump(buff))

	count, err = sockConn.Read(buff)
	fmt.Printf("Socket read count %d ---\n %s\n", count, buff)
	log.Printf("-------REQ-------\n%s\n                    ------------------\n", hex.Dump(buff))

	sockConn.Close()
}
