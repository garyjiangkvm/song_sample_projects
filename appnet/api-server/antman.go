package main

import (
	"fmt"
	"github.com/ant0ine/go-json-rest/rest"
	"github.com/sluu99/uuid"
	"log"
	"net/http"
	"sync"
)

func main() {

	nstore := Network_store{
		Store: map[string]*Network{},
	}

	api := rest.NewApi()
	api.Use(rest.DefaultDevStack...)
	router, err := rest.MakeRouter(
		rest.Get("/api/appsoar-network/config", GetConfig),
		rest.Get("/api/appsoar-network/networktype", GetNetworkType),
		rest.Get("/api/appsoar-network/container", GetContainers),
		rest.Get("/api/appsoar-network/network", nstore.GetNetworkPool),
		rest.Post("/api/appsoar-network/network", nstore.PostNetwork),
		rest.Get("/api/appsoar-network/network/:id", nstore.GetNetworkUuid),
		rest.Put("/api/appsoar-network/network/:id", nstore.PutNetworkUuid),
		rest.Delete("/api/appsoar-network/network/:id", nstore.DeleteNetworkUuid),
	)
	if err != nil {
		log.Fatal(err)
	}
	api.SetApp(router)
	log.Fatal(http.ListenAndServe(":8081", api.MakeHandler()))
}

type Config struct {
	Content struct {
		Kind          string `json:"type"`
		Cluster_store string `json:"cluster_store"`
		Node0_ip_port string `json:"node0_ip_port"`
		Node1_ip_port string `json:"node1_ip_port"`
		Node2_ip_port string `json:"node2_ip_port"`
	} `json:"content"`
	Message string `json:"message"`
	Code    string `json:"code"`
}

func GetConfig(w rest.ResponseWriter, r *rest.Request) {
	var config Config

	config.Content.Kind = "etcd"
	config.Content.Cluster_store = "etcd://192.168.14.62:2379"
	config.Content.Node0_ip_port = "192.168.14.62:2379"
	config.Message = "Configuration"
	config.Code = "0"
	fmt.Println(&config)
	w.WriteJson(&config)
}

type Network_type struct {
	Content []map[string]string `json:"content"`
	Message string              `json:"message"`
	Code    string              `json:"code"`
}

func GetNetworkType(w rest.ResponseWriter, r *rest.Request) {
	nt := Network_type{
		Content: []map[string]string{{"type": "docker", "desc": "docker native network mode"}, {"type": "ipsec", "desc": "ipsec overlay network"}},
		Message: "Network Type",
		Code:    "0",
	}

	w.WriteJson(&nt)
}

type Network struct {
	Kind      string `json:"type"`
	Name      string `json:"name"`
	Subnet_ip string `json:"subnet_ip"`
	Uuid      string `json:"uuid"`
}

type Network_uuid struct {
	Content Network `json:"content"`
	Message string  `json:"message"`
	Code    string  `json:"code"`
}

type Network_pool struct {
	Content []Network `json:"content"`
	Message string    `json:"message"`
	Code    string    `json:"code"`
}

type Network_store struct {
	sync.RWMutex
	Store map[string]*Network
}

type Response struct {
	Message string `json:"message"`
	Code    string `json:"code"`
}

func GoodResponse(method string, r *Response) *Response {
	r.Message = method + " successful"
	r.Code = "0"
	return r
}

func (s *Network_store) GetNetworkUuid(w rest.ResponseWriter, r *rest.Request) {
	id := r.PathParam("id")
	s.RLock()
	var network *Network
	if s.Store[id] != nil {
		network = &Network{}
		*network = *s.Store[id]
	}
	s.RUnlock()
	if network == nil {
		rest.NotFound(w, r)
		return
	}

	var n_uuid Network_uuid
	n_uuid.Content = *network
	n_uuid.Message = "Network UUID"
	n_uuid.Code = "0"
	w.WriteJson(&n_uuid)
}

func (s *Network_store) GetNetworkPool(w rest.ResponseWriter, r *rest.Request) {
	s.RLock()
	var n_pool Network_pool
	n_pool.Content = make([]Network, len(s.Store))
	i := 0
	for _, network := range s.Store {
		n_pool.Content[i] = *network
		i++
	}
	s.RUnlock()

	n_pool.Message = "Network Pool"
	n_pool.Code = "0"
	w.WriteJson(&n_pool)
}

func (s *Network_store) PostNetwork(w rest.ResponseWriter, r *rest.Request) {
	network := Network{}
	err := r.DecodeJsonPayload(&network)
	if err != nil {
		rest.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	s.Lock()
	var uu uuid.UUID = uuid.Rand()
	id := uu.Hex()
	network.Uuid = id
	s.Store[id] = &network
	s.Unlock()

	resp := Response{}
	w.WriteJson(GoodResponse("POST", &resp))
}

func (s *Network_store) PutNetworkUuid(w rest.ResponseWriter, r *rest.Request) {
	id := r.PathParam("id")
	s.Lock()
	if s.Store[id] == nil {
		rest.NotFound(w, r)
		s.Unlock()
		return
	}
	network := Network{}
	err := r.DecodeJsonPayload(&network)
	if err != nil {
		rest.Error(w, err.Error(), http.StatusInternalServerError)
		s.Unlock()
		return
	}
	network.Uuid = id
	s.Store[id] = &network
	s.Unlock()

	resp := Response{}
	w.WriteJson(GoodResponse("PUT", &resp))
}

func (s *Network_store) DeleteNetworkUuid(w rest.ResponseWriter, r *rest.Request) {
	id := r.PathParam("id")
	s.Lock()
	delete(s.Store, id)
	s.Unlock()

	resp := Response{}
	w.WriteJson(GoodResponse("DELETE", &resp))
}

//container
type Containers struct {
	Content []map[string]string `json:"content"`
	Message string              `json:"message"`
	Code    string              `json:"code"`
}

func GetContainers(w rest.ResponseWriter, r *rest.Request) {
	c := Containers{
		Content: []map[string]string{{"type": "web", "name": "nginx", "network_name": "net0", "host_ip": "10.42.0.1/16", "uuid": "8f07c27d-28d1-43b2-88d4-346f000b249e"}, {"type": "database", "name": "mysql", "network_name": "net1", "host_ip": "10.42.18.1/16", "uuid": "b4f257dc-8fc5-4f81-86b8-fd8b8b21ad54"}},
		Message: "Containers",
		Code:    "0",
	}

	w.WriteJson(&c)
}
