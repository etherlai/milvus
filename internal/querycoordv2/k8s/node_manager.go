package k8s

import (
	"encoding/json"
	"io"
	"net/http"
	"os"
)

type K8sInfoManager struct {
	ServerAddr string
}

func NewK8sInfoManager() *K8sInfoManager {
	addr := os.Getenv("MILVUS_K8S_SERVER_ADDR")
	return &K8sInfoManager{
		ServerAddr: addr,
	}
}

type QueryNodeK8sInfo struct {
	PodName   string            `json:"podName"`
	Addr      string            `json:"addr"`
	Selectors map[string]string `json:"selectors,omitempty"`
	K8sNode   string            `json:"k8sNode"`
}

func (k *K8sInfoManager) GetAllQueryNodes() ([]*QueryNodeK8sInfo, error) {
	resp, err := http.Get(k.ServerAddr + "/querynodes")
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	qns := make([]*QueryNodeK8sInfo, 0)
	if err := json.Unmarshal(body, qns); err != nil {
		return nil, err
	}
	return qns, nil
}
