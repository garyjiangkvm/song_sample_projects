# A Simple Joke Service

## Usage

Get golang package __github.com/gorilla/mux__

Build and run.

`go build joke-service.go;sudo ./joke-service
`

## Access point

This service support both unix domain socket and ip:port access. 

- `/var/run/tigera/joke-service.sock`
- `localhost:5000`

## API definitions
    URL                           HTTP Method  Operation
    / or /v1/                     GET          Get a joke for a random name
    /stat or /v1/stat             GET          Return service statistics
   

## Test

To run unit test

`go test -v`


