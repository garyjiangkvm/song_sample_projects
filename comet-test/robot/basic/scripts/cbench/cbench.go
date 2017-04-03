package main

import (
	"fmt"
	"math/rand"
	"os"
	"os/signal"
	"syscall"

	"time"

	"github.com/urfave/cli"
)

const (
	SIZE_1M  = 1048576
	SIZE_4M  = 4194304
	SIZE_1G  = 1073741824
	OK       = "ok"
	CONTINUE = "continue"
)

var (
	woffset  int64 = 0
	count    int64 = 0
	data     []byte
	stop     bool = false
	async    bool = false
	DataSize int  = ((SIZE_4M + rand.Intn(SIZE_4M)) >> 12) << 12
)

func main() {
	BenchFlags := []cli.Flag{
		cli.Int64Flag{
			Name:  "times",
			Usage: "specific benchmark scan disk times",
		},
		cli.Int64Flag{
			Name:  "seconds",
			Usage: "specific benchmark run seconds",
		},
		cli.StringFlag{
			Name:  "device",
			Usage: "specific benchmark device",
		},
		cli.BoolFlag{
			Name:  "async",
			Usage: "specific async read and write",
		},
	}

	benchCmd := cli.Command{
		Name:   "block",
		Usage:  "benchmark block device",
		Flags:  BenchFlags,
		Action: Bench,
	}

	app := cli.NewApp()
	app.Name = "cbench"
	app.Usage = "cbench [args]"
	app.Version = "0.3.0"
	app.Commands = []cli.Command{
		benchCmd,
	}

	app.Run(os.Args)
}

func Bench(c *cli.Context) {
	var err error
	device, err := GetFlag(c, "device", true, err)
	if err != nil {
		return
	}

	times := c.Int64("times")
	seconds := c.Int64("seconds")
	async = c.Bool("async")
	if times == 0 && seconds == 0 {
		times = 1
		seconds = 30
	}

	capacity, err := GetDiskSize(device)
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Printf("Start to benchmark %s with capacity %dM.(times %d seconds %d)\n", device, capacity/SIZE_1M, times, seconds)

	err = InitData()
	if err != nil {
		fmt.Println(err)
		return
	}

	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, os.Interrupt, syscall.SIGINT, os.Kill, syscall.SIGTERM)
	start := time.Now().Unix()

	exit := make(chan string, 20)
	go _WriteRoutineOnce(device, exit)
	go _ReadRoutineOnce(device, exit)

	for {
		select {
		case msg := <-exit:
			if msg == OK {
				t := time.Now().Unix() - start
				b := float64(count*capacity+woffset) / SIZE_1G
				bw := float64(count*capacity+woffset) / float64(SIZE_1M*t)
				fmt.Printf("\n\n[Result]\nDevice:\t%s \nTime:\t%ds\nBytes:\t%f GB, \nBW:\t%f MB/s\nScan:\t%d times\nLook Good!\n\n", device, t, b, bw, count)
				stop = true
			} else if msg == CONTINUE {
				if times > 0 && count+1 >= times {
					exit <- OK
				}
				err = InitData()
				if err != nil {
					fmt.Println(err)
					return
				}
				count++
				go _WriteRoutineOnce(device, exit)
				time.Sleep(2 * time.Second)
				go _ReadRoutineOnce(device, exit)
				continue
			} else {
				t := time.Now().Unix() - start
				b := float64(count*capacity+woffset) / SIZE_1G
				bw := float64(count*capacity+woffset) / float64(SIZE_1M*t)
				fmt.Printf("\n\n[Result]\nDevice:\t%s \nTime:\t%ds\nBytes:\t%f GB, \nBW:\t%f MB/s\nScan:\t%d times\nLook Good!\n\n", device, t, b, bw, count)
				fmt.Println(msg)
			}

			return
		case <-time.After(2 * time.Second):
			var percent int = 0
			if seconds != 0 {
				lease := time.Now().Unix() - start
				if lease >= seconds {
					//fmt.Println("timeout exit: ", seconds)
					stop = true
					percent = 100
				} else {
					percent = int(lease * 100 / seconds)
				}
			}

			if times != 0 {
				tp := int(((count*capacity + woffset) * 100) / (times * capacity))
				if percent < tp {
					percent = tp
				}
			}

			b := float64(count*capacity+woffset) / SIZE_1G
			//fmt.Printf("count:%d, capacity:%d, woffset:%d DataSize:%d\n", count, capacity, woffset, DataSize)
			fmt.Printf("Check %f GB [%d%%]\r", b, percent)
		case sig := <-sigs:
			fmt.Println("receive signal: ", sig)
			stop = true
		}
	}
}

func _WriteRoutineOnce(path string, exit chan string) {
	var flag int = os.O_RDWR | os.O_APPEND
	if !async {
		flag |= os.O_SYNC
	}

	fi, err := os.OpenFile(path, flag, 0666)
	if err != nil {
		panic(err)
	}
	defer fi.Close()

	capacity, err := GetDiskSize(path)
	if err != nil {
		fmt.Printf("Usage: %s [threads] [seconds] [device]\n", os.Args[0])
		return
	}

	woffset = 0
	for !stop {
		var n int = 0
		var err error
		if woffset+int64(len(data)) > capacity {
			size := capacity - woffset
			n, err = fi.WriteAt(data[:size], woffset)
			if err != nil {
				//fmt.Printf("write path:%s error:%v", path, err)
				exit <- "Write Error"
				return
			}

			if int64(n) != size {
				exit <- "Write Error"
				return
			}

			// write disk full, exit
			woffset = capacity
			return
		} else {
			n, err = fi.WriteAt(data, woffset)
			if err != nil {
				//fmt.Printf("write path:%s error:%v", path, err)
				exit <- err.Error()
				return
			}
			//fmt.Printf("woffset:%d DataSize:%d n:%d\n", woffset, DataSize, n)
			woffset += int64(DataSize)
		}
		if n != DataSize {
			exit <- "Write Data not enough"
			return
		}
	}
}

func _ReadRoutineOnce(path string, exit chan string) {
	var flag int = os.O_RDONLY | os.O_APPEND
	if !async {
		flag |= os.O_SYNC
	}
	fi, err := os.OpenFile(path, flag, 0666)
	if err != nil {
		panic(err)
	}
	defer fi.Close()

	capacity, err := GetDiskSize(path)
	if err != nil {
		fmt.Printf("Usage: %s [threads] [seconds] [device]\n", os.Args[0])
		return
	}

	var offset int64 = 0
	var size int64 = 0
	var strPos int = 0

	buf := make([]byte, DataSize)
	for !stop {
		size = 0
		if woffset-offset >= 1024 || woffset == capacity {
			size = woffset - offset
		} else {
			time.Sleep(20 * time.Millisecond)
			continue
		}

		if size > int64(DataSize) {
			size = int64(DataSize)
		}
		buffer := buf[:size]
		n, err := fi.ReadAt(buffer, offset)
		if err != nil {
			fmt.Printf("write path:%s error:%v", path, err)
			exit <- "Read Error"
			return
		}

		var i int64
		for i = 0; i < int64(n); i++ {
			//fmt.Printf("char:%d i:%d\n",buffer[i], i)
			//fmt.Printf("data:%d p:%d\n",data[strPos], strPos)
			if buffer[i] != data[strPos] {
				exit <- fmt.Sprintf("Data Inconsistency: offset:%d[%d] Pos:%d[%d]", offset+i, buffer[i], strPos, data[strPos])
				return
			}
			strPos++
			if strPos >= len(data) {
				strPos = 0
			}
		}

		offset += int64(n)
		if offset >= capacity {
			exit <- CONTINUE
			//read full disk
			return
		}
	}

	exit <- OK
}

func InitData() error {
	fi, err := os.OpenFile("/dev/urandom", os.O_RDWR|os.O_APPEND, 0666)
	if err != nil {
		panic(err)
	}
	defer fi.Close()

	DataSize = ((SIZE_4M + rand.Intn(SIZE_4M)) >> 12) << 12
	data = make([]byte, DataSize)
	n, err := fi.Read(data)
	if err != nil {
		fmt.Println(err)
		return err
	}

	if n != DataSize {
		return fmt.Errorf("prepare data size:%d not enough", n)
	}

	fw, err := os.OpenFile("/tmp/data", os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		panic(err)
	}
	defer fw.Close()
	n, err = fw.Write(data)
	if n != DataSize {
		return fmt.Errorf("prepare data write size:%d not enough", n)
	}

	//fmt.Printf("Init random data size:%d", n)
	return nil
}

func _WriteRoutine(path string, data []byte, exit chan string) {
	fi, err := os.OpenFile(path, os.O_RDWR|os.O_SYNC|os.O_APPEND, 0666)
	if err != nil {
		panic(err)
	}
	defer fi.Close()

	capacity, err := GetDiskSize(os.Args[3])
	if err != nil {
		fmt.Printf("Usage: %s [threads] [seconds] [device]\n", os.Args[0])
		return
	}

	woffset = 0
	for !stop {
		var n int = 0
		var err error
		if woffset+int64(len(data)) > capacity {
			size := capacity - woffset
			n, err = fi.WriteAt(data[:size], woffset)
			if err != nil {
				//fmt.Printf("write path:%s error:%v", path, err)
				exit <- "Write Error"
				return
			}

			woffset = 0
			count++
			var nn int = 0
			nn, err = fi.WriteAt(data[size:], woffset)
			if err != nil {
				//fmt.Printf("write path:%s error:%v", path, err)
				exit <- "Write Error"
				return
			}
			woffset += int64(nn)
			n += nn
		} else {
			n, err = fi.WriteAt(data, woffset)
			if err != nil {
				//fmt.Printf("write path:%s error:%v", path, err)
				exit <- "Write Error"
				return
			}
			woffset += int64(DataSize)
		}
		if n != DataSize {
			exit <- "Write Data not enough"
			return
		}
	}

	exit <- OK
}

func _ReadRoutine(path string, data []byte, exit chan string) {
	fi, err := os.OpenFile(path, os.O_SYNC|os.O_RDONLY, 0666)
	if err != nil {
		panic(err)
	}
	defer fi.Close()

	capacity, err := GetDiskSize(os.Args[3])
	if err != nil {
		fmt.Printf("Usage: %s [threads] [seconds] [device]\n", os.Args[0])
		return
	}

	var offset int64 = 0
	var rcount int64 = 0
	var size int64 = 0
	var strPos int = 0

	buf := make([]byte, DataSize)
	for !stop {
		size = 0
		if count == rcount && woffset-offset < 1024 {
			time.Sleep(20 * time.Millisecond)
			continue
		} else if count == rcount && woffset-offset >= 1024 {
			size = woffset - offset
		} else if count > rcount && woffset-offset < 1024 {
			size = capacity - offset
		} else if count > rcount && woffset-offset >= 1024 {
			size = woffset - offset
		} else {
			panic("read faster than write impossible!")
		}
		if size > int64(DataSize) {
			size = int64(DataSize)
		}
		buffer := buf[:size]
		n, err := fi.ReadAt(buffer, offset)
		if err != nil {
			fmt.Printf("write path:%s error:%v", path, err)
			exit <- "Read Error"
			return
		}

		var i int64
		for i = 0; i < int64(n); i++ {
			//fmt.Printf("char:%d i:%d\n",buffer[i], i)
			//fmt.Printf("data:%d p:%d\n",data[strPos], strPos)
			if buffer[i] != data[strPos] {
				exit <- fmt.Sprintf("Data Inconsistent: offset:%d[%s] Pos:%d[%s]", offset+i, buffer[i], strPos, data[strPos])
				return
			}
			strPos++
			if strPos >= len(data) {
				strPos = 0
			}
		}

		offset += int64(n)
		if offset >= capacity {
			offset = 0
			rcount++
		}
	}

	exit <- OK
}

func GetDiskSize(disk string) (int64, error) {
	var size int64
	fd, err := os.Open(disk)
	if err != nil {
		return 0, err
	}
	defer fd.Close()
	fi, err := fd.Stat()
	if err != nil {
		return 0, err
	}
	if fi.Mode()&os.ModeDevice == os.ModeDevice {
		size, err = fd.Seek(0, 2)
		if err != nil {
			return 0, err
		}
		fd.Seek(0, 0)
	} else {
		return 0, fmt.Errorf("the device %s is not a block device", disk)
	}
	return size, nil
}

func RequiredMissingError(name string) error {
	return fmt.Errorf("Cannot find valid required parameter: %s", name)
}

func GetFlag(v interface{}, key string, required bool, err error) (string, error) {
	if err != nil {
		return "", err
	}
	value := ""
	switch v := v.(type) {
	default:
		return "", fmt.Errorf("Unexpected type for getLowerCaseFlag")
	case *cli.Context:
		if key == "" {
			value = v.Args().First()
		} else {
			value = v.String(key)
		}
	case map[string]string:
		value = v[key]
	}
	if required && value == "" {
		err = RequiredMissingError(key)
	}
	return value, err
}
