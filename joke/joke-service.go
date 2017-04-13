package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
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
	SOCKFILE = "/var/run/names/joke-service.sock"
	IPPORT   = ":5000"
	TIMEOUT  = 5 //seconds
)

var (
	ErrMsg   = "Resource Limit Is Reached"
	ErrLimit = errors.New("Resource Limit Is Reached.")

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

func fetchWithTimeout(id string, url string, timeout int) (string, error) {
	tm := time.Duration(timeout) * time.Second
	c := &http.Client{
		Timeout: tm,
	}

	count := 0
	for {
		res, err := c.Get(url)
		if err != nil {
			return "", err
		}
		body, err := ioutil.ReadAll(res.Body)
		res.Body.Close()
		if err != nil {
			return "", err
		}

		if id == "name" {
			var p Person
			if err = json.Unmarshal([]byte(body), &p); err != nil {
				/* If the name server http://uinames.com/api/ is under pressure, sometimes it will return StatusOK with Error Msgs.
				   I see couple of those messages as followings.
				       Resource Limit Is Reached
				       Out of Memory
				       The requested URL /500 was not found on this server
				    We can retry number of times before we finally give up.
				*/

				if count > 10 {
					log.Println(ErrLimit.Error())
					return "", ErrLimit
				}
				<-time.After(1 * time.Second)
				count++
			} else {
				return string(body), nil
			}
		} else {
			return string(body), nil
		}
	}
}

func (s *serviceContext) getJokeHandlerV1(w http.ResponseWriter, req *http.Request) {
	var p Person
	var j Joke

	s.Stat.TotalReq++
	datac, errc := make(chan map[string]string), make(chan error)

	for id, url := range s.urls {
		go func(id string, url string) {
			body, err := fetchWithTimeout(id, url, s.timeout)
			if err != nil {
				errc <- err
				return
			}
			data := make(map[string]string)
			data[id] = string(body)
			datac <- data
		}(id, url)
	}

	for i := 0; i < len(urls); i++ {
		select {
		case val := <-datac:

			if body, ok := val["name"]; ok {
				if err := json.Unmarshal([]byte(body), &p); err != nil {
					s.Stat.ErrReq++
					log.Println("Http get name json content error - ", err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}

			}

			if body, ok := val["joke"]; ok {
				if err := json.Unmarshal([]byte(body), &j); err != nil {
					s.Stat.ErrReq++
					log.Println("Http get joke json content error - ", err)
					http.Error(w, err.Error(), http.StatusInternalServerError)
					return
				}

			}

		case err := <-errc:
			s.Stat.ErrReq++
			log.Println("Http get contents error - ", err)
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
	}

	//When we reach here, all URLs should be processed and parsed correctly.

	//Replace surname and name seperately to cover cases like "JoneDoe" or "Jone-Doe"
	msg := strings.Replace(j.Value.Joke, setName, p.Name, -1)
	msg = strings.Replace(msg, setSurname, p.Surname, -1)

	w.WriteHeader(http.StatusOK)
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
