package main

import (
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"syscall"
	"time"

	notify "./notifications"
	"github.com/golang/protobuf/proto"
	"github.com/gorilla/mux"
	"github.com/nats-io/go-nats"
)

const (
	VERSION  = "1.0"
	SOCKFILE = "/var/run/storageos/dataplane-notifications.sock"
	IPPORT   = ":1337"
	INTERVAL = 5 //seconds
)

var (
	eventChannel chan notify.Error
)

type timeEvent struct {
	Stamp time.Time    `json:"stamp,omitempty"`
	Event notify.Error `json:"event,omitempty"`
}

type batchEvents struct {
	EventMarker map[string]uint32 //put a marker into EventMarker to show that an event with (id+subject) has been recieved into this batch
	Events      []timeEvent
}

type notifyContext struct {
	Version  string `json:"version,omitempty"`
	SockFile string `json:"socket,omitempty"`
	IpPort   string `json:"ipport,omitempty"`
	Interval int    `json:"interval,omitempty"`
	Stat     struct {
		TotalReq          int            `json:"total_req"`
		TotalBadReq       int            `json:"total_bad_req"`
		TotalEvent        int            `json:"total_event"`
		TotalPublishEvent int            `json:"total_publish_event"`
		TotalBatch        int            `json:"total_batch"`
		Subject           map[string]int `json:"subject,omitempty"` //events count published for a subject
	} `json:"stat"`
	Message string `json:"message,omitempty"`
	Code    string `json:"code,omitempty"`
}

func checkSockFile(sock string) error {
	path := filepath.Dir(sock)
	if _, err := os.Stat(path); os.IsNotExist(err) {
		if err := os.MkdirAll(path, os.ModeDir|0700); err != nil {
			return err
		}
	}

	if _, err := os.Stat(sock); err == nil {
		log.Println("Remove previous sockfile at %v", sock)
		if err := os.Remove(sock); err != nil {
			return err
		}
	}

	return nil
}

func listenAndServeWithClose(sock string, addr string, handler http.Handler) (ssc io.Closer, tsc io.Closer, err error) {

	sockListener, err := net.Listen("unix", sock)
	if err != nil {
		return nil, nil, err
	}

	tcpListener, err := net.Listen("tcp", addr)
	if err != nil {
		return nil, nil, err
	}

	go func() {
		log.Println("Server listen on ", sock)
		err := http.Serve(sockListener, handler)
		if err != nil {
			log.Println("Sock HTTP Server Error - ", err)
		}
	}()

	go func() {
		log.Println("Server listen on ", addr)
		err := http.Serve(tcpListener, handler)
		if err != nil {
			log.Println("Tcp HTTP Server Error - ", err)
		}
	}()

	return sockListener, tcpListener, nil
}

func (n *notifyContext) GetNotifyContextV1(w http.ResponseWriter, req *http.Request) {
	n.Message = "Notify Context"
	n.Code = "0"
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(n)

}

func (n *notifyContext) PostErrorHandlerV1(w http.ResponseWriter, req *http.Request) {
	body, err := ioutil.ReadAll(req.Body)
	if err != nil {
		log.Fatalf("ERROR: %s", err)
	}

	log.Printf("-------REQ-------\n%s\n                    ------------------\n", hex.Dump(body))

	newEvent := &notify.Error{}
	err = proto.Unmarshal(body, newEvent)
	if err != nil {
		log.Fatal("Protobuf unmarshaling error: ", err)
	}
	log.Printf("\nPost Send id is %d. subject is %s\n", newEvent.GetId(), newEvent.GetSubject())

	if newEvent.GetSubject() == "" {
		w.WriteHeader(http.StatusBadRequest)
		n.Stat.TotalBadReq++

	} else {
		eventChannel <- *newEvent
		w.WriteHeader(http.StatusOK)

	}

	n.Stat.TotalReq++
	w.Write([]byte("HTTP status code returned!"))

}

// Add v1 endpoint handler function for /v1/errors
func (n *notifyContext) SetupV1Routes(r *mux.Router) {
	r.HandleFunc("/errors/stat", n.GetNotifyContextV1).Methods("GET")
	r.HandleFunc("/errors", n.PostErrorHandlerV1).Methods("POST")
}

//Batch worker recieve events and put it into event queue. It fires a Nats publisher every 5 seconds.
func (n *notifyContext) BatchWorker(c chan notify.Error) error {
	var key string
	var be batchEvents
	var newEvent notify.Error
	be.EventMarker = make(map[string]uint32)

	natsCon, _ := nats.Connect(nats.DefaultURL)
	defer natsCon.Close()

	for {
		select {
		case newEvent = <-c:
			n.Stat.TotalEvent++
			id := newEvent.GetId()
			subject := newEvent.GetSubject() //When we reach here, subject is not nil. Checked by Post handler.
			log.Printf("\nBatch Get id is %d. subject is %s. Marker %d\n", id, subject, be.EventMarker[subject])

			key = fmt.Sprintf("%d-%s", id, subject)
			if be.EventMarker[key] == 0xdeadbeef {
				//we already have this id+subject marker, dont add it again.
				log.Printf("\nDuplicated key (%s) event found. Skip this one.\n", key)
				continue
			}

			be.EventMarker[key] = 0xdeadbeef //add one because it could be 0 when no key found
			be.Events = append(be.Events, timeEvent{Stamp: time.Now(), Event: newEvent})
			log.Printf("Add Event id is %d. subject is %s\n", id, subject)
			log.Print(be.Events)

			n.Stat.Subject[subject]++

		case <-time.After(time.Duration(n.Interval) * time.Second):
			if len(be.Events) == 0 {
				//log.Println("no more events to send", count)
				continue
			}
			payload, err := json.Marshal(be.Events)
			if err != nil {
				log.Println("json Marshall Error - ", err)
				continue

			}

			msg := &nats.Msg{Subject: "notify-error", Reply: "OK", Data: []byte(payload)}
			natsCon.PublishMsg(msg)
			n.Stat.TotalPublishEvent += len(be.Events)

			//Clear out batch events. Let GC to recycle used memory
			be.Events = nil
			be.EventMarker = make(map[string]uint32)

			n.Stat.TotalBatch++

		}
	}
	return nil
}

func main() {

	nc := &notifyContext{
		Version:  VERSION,
		SockFile: SOCKFILE,
		IpPort:   IPPORT,
		Interval: INTERVAL,
	}
	nc.Stat.Subject = make(map[string]int)

	fmt.Println("Error event server ", VERSION)

	eventChannel = make(chan notify.Error)
	go nc.BatchWorker(eventChannel)

	router := mux.NewRouter()
	nc.SetupV1Routes(router.PathPrefix("/v1").Subrouter())

	err := checkSockFile(nc.SockFile)
	if err != nil {
		log.Fatal("CheckSockFile Error - ", err)
	}

	// Start two listeners
	sockCloser, tcpCloser, err := listenAndServeWithClose(nc.SockFile, nc.IpPort, router)
	if err != nil {
		log.Fatalln("ListenAndServeWithClose Error - ", err)
	}

	// Wait for signal
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, os.Interrupt, syscall.SIGINT, os.Kill, syscall.SIGTERM)
	s := <-sigs
	log.Println("Receive signal: ", s)

	// Close HTTP Server
	err = sockCloser.Close()
	if err != nil {
		log.Fatalln("Sock Server Close Error - ", err)
	}
	err = tcpCloser.Close()
	if err != nil {
		log.Fatalln("Tcp Server Close Error - ", err)
	}

	log.Println("Server Closed")

}
