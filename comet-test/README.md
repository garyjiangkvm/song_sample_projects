# Comet安装操作测试手册
### Comet是有容云推出的针对容器平台的存储产品。它主要由以下3个组件组成。
| 组件 | 功能 | 后端可执行文件 | 运行示例 |
|--------|--------|
| 驱动层 |  在容器主机上运行，为Docker Engine提供存储驱动Plugin | codrv | codrv  daemon |
| 策略层 |  在高可用元数据主机上运行，为整个存储集群提供元数据，健康检查，策略调度等服务 | cosrv | cosrv daemon |
| 引擎层 |  分为两部分，一部分在存储主机上运行，将主机的存储资源提供给整个集群使用，另外一部分在容器主机上运行，负责为容器对接实际的存储资源，提供IO访问通道和数据处理 | costor | costor engine/controller |

###存储访问流程
_ _ _

1. Docker run以codrv为存储卷Plugin创建容器。
2. codrv接受到创建卷的request,发送给策略层cosrv。
3. 策略层cosrv根据相应的调度策略为此request分配存储资源，可能涉及发送多个request发送给不同存储主机上的引擎层。
4. 引擎层根据接受到的策略层request在存储主机上分配存储资源，并且反馈结果给策略层engine服务。
5. 策略层返回存储资源分配结果给驱动层，并且记录相关容器-存储资源的元数据。
6. 驱动层拿到策略层返回的存储资源结果后，发送request到同主机的引擎层controller服务, 创建实际的块设备。
7. 驱动层对于新创建的块设备进行格式化，然后为容器挂载卷。

###驱动层的安装运行
驱动层的安装运行由两部分组成。分两个步骤运行。
- codrv.spec是驱动层的配置文件。
- codrv执行文件。

- [ ] 在/etc/docker/plugins下创建codrv.spec
```
echo "unix:///var/run/codrv/codrv.sock" > /etc/docker/plugins/codrv.spec
```

- [ ] 运行codrv
```
nohup codrv daemon --conf codrv.conf &
```

###策略层的安装运行
策略层的安装运行由两部分组成。分两个步骤运行。
- 安装运行etcd集群。
- 安装运行cosrv策略层服务。

- [ ] 安装好etcd后，运行
```
sh ./startEtcd.sh
```
或者直接运行
```
etcd -name etcd0 -initial-advertise-peer-urls http://192.168.14.200:2380 \
    -listen-peer-urls http://192.168.14.200:2380 \
    -listen-client-urls http://192.168.14.200:2379,http://127.0.0.1:2379 -advertise-client-urls    http://192.168.14.200:2379 \
     -initial-cluster-token etcd-cluster \
     -initial-cluster etcd0=http://192.168.14.200:2380,etcd1=http://192.168.14.201:2380,etcd2=http://192.168.14.202:2380 \
     -initial-cluster-state new
```
- [ ] 运行cosrv
```
nohup cosrv daemon --conf comet_cluster_server.conf &
```

###引擎层的安装运行
引擎层分别包含在容器主机上和驱动层相关的controller部分，以及在存储主机上的存储engine部分

- [ ] 在容器主机上运行controller
```
nohup costor controller --conf costor.conf &
```

- [ ] 在存储主机上运行engine
```
nohup costor engine --conf costor.conf &
```
