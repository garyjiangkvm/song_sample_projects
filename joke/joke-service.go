package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"github.com/gorilla/mux"
)

const (
	VERSION  = "1.0"
	SOCKFILE = "/var/run/tigera/joke-service.sock"
	IPPORT   = ":5000"
	TIMEOUT  = 5 //seconds
)

var (
	ErrTimeout = errors.New("Server takes too long to respond.")
	setName    = "John"
	setSurname = "Doe"
	urls       = map[string]string{
		"name": "http://uinames.com/api/",
		"joke": "http://api.icndb.com/jokes/random?firstName=John&lastName=Doe&limitTo=[nerdy]",
	}
)

type Person struct {
	Name    string `json:"name"`
	Surname string `json:"surname"`
	Gender  string `json:"gender"`
	Region  string `json:"region"`
}

type Joke struct {
	Type  string `json:"type"`
	Value struct {
		Id         int      `json:"id"`
		Joke       string   `json:"joke"`
		Categories []string `json:"categories"`
	} `json:"value"`
}

type httpResponse struct {
	id       string
	url      string
	response *http.Response
	err      error
}

type serviceContext struct {
	Version  string `json:"version,omitempty"`
	SockFile string `json:"socket,omitempty"`
	IpPort   string `json:"ipport,omitempty"`

	timeout int
	urls    map[string]string

	Stat struct {
		TotalReq int `json:"total_req"`
		ErrReq   int `json:"error_req"`
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

//A genearic function to listen on socket and ip:port. io.Closer will be returned.
func listenAndServeWithClose(sock string, addr string, handler http.Handler) (io.Closer, io.Closer, error) {

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
			log.Fatalln("Tcp HTTP Server Error - ", err)
		}
	}()

	return sockListener, tcpListener, nil
}

//A generic function to get URL content asynchronously. Put everything in a response array when it is done.
//We use this function to facilitate future extension for more urls. Caller will check the error and close resp.Body.
func asyncHttpGets(urls map[string]string, timeout int) ([]*httpResponse, error) {
	tm := time.Duration(timeout) * time.Second
	ch := make(chan *httpResponse, len(urls)) // buffered channel
	responses := []*httpResponse{}

	for i, url := range urls {
		go func(urlID string, url string) {
			c := &http.Client{
				Timeout: tm,
			}
			resp, err := c.Get(url)
			ch <- &httpResponse{urlID, url, resp, err}
		}(i, url)
	}

	for {
		select {
		case r := <-ch:
			responses = append(responses, r)

			if len(responses) == len(urls) {
				return responses, nil
			}
		case <-time.After(tm):
			return responses, ErrTimeout
		}
	}

	return responses, nil

}

func (s *serviceContext) getJokeHandlerV1(w http.ResponseWriter, req *http.Request) {
	var p Person
	var j Joke

	s.Stat.TotalReq++
	responses, err := asyncHttpGets(s.urls, s.timeout)
	for _, r := range responses {
		if r.err == nil {
			defer r.response.Body.Close() //make sure we close the body
		}
	}
	if err != nil {
		s.Stat.ErrReq++

		log.Println("Http get contents error - ", err)
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	for _, r := range responses {

		if r.err != nil {
			s.Stat.ErrReq++

			log.Println("Http get contents error on url ", r.url, r.err)
			http.Error(w, r.err.Error(), http.StatusInternalServerError)
			return
		}

		if r.id == "name" {
			if err = json.NewDecoder(r.response.Body).Decode(&p); err != nil {
				log.Println("Http parse response error on url ", r.url, err)
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return

			}

		}

		if r.id == "joke" {
			if err = json.NewDecoder(r.response.Body).Decode(&j); err != nil {
				log.Println("Http parse response error on url ", r.url, err)
				http.Error(w, err.Error(), http.StatusInternalServerError)
				return

			}

		}

	}

	//When we reach here, all URLs should be processed and parsed correctly.

	//Replace surname and name seperately to cover cases like "JoneDoe" or "Jone-Doe"
	msg := strings.Replace(j.Value.Joke, setName, p.Name, -1)
	msg = strings.Replace(msg, setSurname, p.Surname, -1)

	w.Write([]byte(msg))

}

func (s *serviceContext) getServiceContextHandlerV1(w http.ResponseWriter, req *http.Request) {
	s.Message = "Service Context"
	s.Code = "0"
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(s)

}

// Add v1 endpoint handler function for /v1 and /v1/stat
func (s *serviceContext) setupV1Routes(r *mux.Router) {
	r.HandleFunc("/stat", s.getServiceContextHandlerV1).Methods("GET")
	r.HandleFunc("/", s.getJokeHandlerV1).Methods("GET")
}

// Add endpoint handler function for / and /stat
func (s *serviceContext) setupRoutes(r *mux.Router) {
	r.HandleFunc("/stat", s.getServiceContextHandlerV1).Methods("GET")
	r.HandleFunc("/", s.getJokeHandlerV1).Methods("GET")
}

func main() {

	sc := &serviceContext{
		Version:  VERSION,
		SockFile: SOCKFILE,
		IpPort:   IPPORT,
		timeout:  TIMEOUT,
		urls:     urls,
	}

	fmt.Println("Tigera Joke Server ", VERSION)

	router := mux.NewRouter()
	sc.setupRoutes(router)
	sc.setupV1Routes(router.PathPrefix("/v1").Subrouter()) //in case we need to expand our API version to /v1 , /v2 etc.

	//check and prepare for new socket file if neccessary
	err := checkSockFile(sc.SockFile)
	if err != nil {
		log.Fatalln("CheckSockFile Error - ", err)
	}

	// Start two listeners
	sockCloser, tcpCloser, err := listenAndServeWithClose(sc.SockFile, sc.IpPort, router)
	if err != nil {
		log.Fatalln("ListenAndServeWithClose Error - ", err)
	}

	// Wait for signal
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, os.Interrupt, syscall.SIGINT, os.Kill, syscall.SIGTERM)
	s := <-sigs
	log.Println("Receive signal: ", s)

	// Close HTTP Servers
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
