package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
)

var (
	tsc = &serviceContext{
		Version:  VERSION,
		SockFile: SOCKFILE,
		IpPort:   IPPORT,
		timeout:  TIMEOUT,
		urls:     urls,
	}
)

func TestAGetJoke(t *testing.T) {

	req, err := http.NewRequest("GET", "/", nil)
	if err != nil {
		t.Fatal(err)
	}

	// We create a ResponseRecorder (which satisfies http.ResponseWriter) to record the response.
	rr := httptest.NewRecorder()
	handler := http.HandlerFunc(tsc.getJokeHandlerV1)

	// Our handlers satisfy http.Handler, so we can call their ServeHTTP method
	// directly and pass in our Request and ResponseRecorder.
	handler.ServeHTTP(rr, req)

	// Check the status code is what we expect.
	if status := rr.Code; status != http.StatusOK {
		t.Errorf("handler returned wrong status code: got %v want %v",
			status, http.StatusOK)
	}

	// Check the response body is what we expect.
	if strings.Contains(rr.Body.String(), setName) || strings.Contains(rr.Body.String(), setSurname) {
		t.Errorf("handler returned unexpected result. No set name should appear. %v",
			rr.Body.String())
	}
}

func TestBTimeout(t *testing.T) {

	tsc.urls["joke"] = "http://10.0.0.0" //a non-reachable url to get http timeout

	req, err := http.NewRequest("GET", "/", nil)
	if err != nil {
		t.Fatal(err)
	}

	// We create a ResponseRecorder (which satisfies http.ResponseWriter) to record the response.
	rr := httptest.NewRecorder()
	handler := http.HandlerFunc(tsc.getJokeHandlerV1)

	// Our handlers satisfy http.Handler, so we can call their ServeHTTP method
	// directly and pass in our Request and ResponseRecorder.
	handler.ServeHTTP(rr, req)

	// Check the status code is what we expect.
	if status := rr.Code; status != http.StatusInternalServerError {
		t.Errorf("handler returned wrong status code: got %v want %v",
			status, http.StatusOK)
	}

	// Check the response body is what we expect.
	if !strings.Contains(rr.Body.String(), "Timeout") {
		t.Errorf("handler returned unexpected for timeout request. %v",
			rr.Body.String())
	}
}

func TestCServiceContext(t *testing.T) {
	var stat serviceContext

	req, err := http.NewRequest("GET", "/stat", nil)
	if err != nil {
		t.Fatal(err)
	}

	// We create a ResponseRecorder (which satisfies http.ResponseWriter) to record the response.
	rr := httptest.NewRecorder()
	handler := http.HandlerFunc(tsc.getServiceContextHandlerV1)

	// Our handlers satisfy http.Handler, so we can call their ServeHTTP method
	// directly and pass in our Request and ResponseRecorder.
	handler.ServeHTTP(rr, req)

	// Check the status code is what we expect.
	if status := rr.Code; status != http.StatusOK {
		t.Errorf("handler returned wrong status code: got %v want %v",
			status, http.StatusOK)
	}

	// Check the response body is what we expect.
	if err := json.Unmarshal([]byte(rr.Body.String()), &stat); err != nil {
		t.Errorf("unmarshall response error")
	}

	if stat.Stat.TotalReq != 2 || stat.Stat.ErrReq != 1 {
		t.Errorf("The statistics of the service is wrong: %v",
			rr.Body.String())
	}
}
