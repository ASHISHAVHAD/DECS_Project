**Roll No: 25M2115**
**Name: Ashish Balasaheb Avhad**
**Subject: CS744 Design and Engineering of Computer Systems**
**Instructor: Prof. Mythili Vutukuru**

GitHub Repository Link: https://github.com/ASHISHAVHAD/DECS_Project

**Project Title:** Performance testing and benchmarking of Key-Value Server and identification of bottlenecks.

**Description:** This project aims to implement and test a functionally correct server, test its performance across various loads and identify software and hardware bottlenecks. The server is a key-value server where user can send a key value to store, access value by providing a key, update value associated with a key and delete a key value pair. The server works over http. The persistent data is stored in mysql server. A cache is also implemented which works as a LRU cache and stores most recently accessed key-value pairs. Cache is in-memory. For concurrency thread pool mechanism is used, where each thread looks for a request in queue and picks up available requests to process if it is idle.

**Technical Details:** Server is implemented in cpp. For server operations, httplib library is used. For storing data mysql server is used. For database connection libmysqlcppconn-dev is used.

Directory Structure:

    |- interactive_client
        |- main.cpp
        |- Makefile

    |- server
        |- cache.cpp
        |- cache.h
        |- config.h
        |- database.cpp
        |- database.h
        |- logger.cpp
        |- logger.h
        |- main.cpp
        |- Makefile
        |- server_app.cpp
        |- server_app.h

    |- create_db.sql
    |- httplib.h
    |- README.md


**Steps to setup and run the project(linux-only):**


1. Install g++ and other essential libraries:

    ```bash
    sudo apt update
    sudo apt install build-essential
    sudo apt install libmysqlcppconn-dev
    ```

2. Install mysql-server:

    ```bash
    sudo apt install mysql-server
    ```

3. Clone this github repository:

    ```bash
    git clone https://github.com/ASHISHAVHAD/DECS_Project.git
    ```

4. Setup mysql-server:

    ```bash
    cd DECS_Project
    sudo mysql < create_db.sql -p
    ```

5. Compile server code:

    ```bash
    cd server
    make
    ```
    This will create an executable named `kv_server` in the `server/` directory.

6. Compile interactive client code:

    ```bash
    cd ../interactive_client
    make
    ```
    This will create an executable named `interactive_client` in the `interactive_client/` directory.

7. Open new terminal window and change current working directory to DECS_Project/server:

    ```bash
    ./kv_server
    ```

8. Open one more terminal and change current working directory to DECS_Project/interactive_client:

    ```bash
    ./interactive_client
    ```

**You can pin server, client and database to different CPU cores to simulate more realistic scenario**

1. For MySQL server run following commands:
```bash
sudo systemctl edit mysql.service
```
Add following to the file, put what cpu cores you want to pin:
```bash
[Service]
CPUAffinity=<list of cpu cores>
```

```bash
sudo systemctl daemon-reload
sudo systemctl restart mysql
```

Verify whether it is pinned properly:
```bash
MYSQL_PID=$(pgrep mysqld | head -n 1)
taskset -cp $MYSQL_PID
```

2. For server:
```bash
taskset -c <list of cpu cores> ./kv_server

3. For interactive client:
```bash
taskset -c ./interactive_client
```

**Sample output of the client is provided below:**

```bash
Enter command (add, get, update, delete, exit): add
Enter key to add: mykey1
Enter value: myvalue for key 1
Request Latency: 0.XXX ms
HTTP Status: 201
Server Response Body:
{"message":"Key-value pair created"}

Enter command (add, get, update, delete, exit): get
Enter key: mykey1
Request Latency: 0.YYY ms
HTTP Status: 200
Server Response Body:
{"key":"mykey1", "value":"myvalue for key 1", "source":"cache"}

Enter command (add, get, update, delete, exit): update
Enter key to update: mykey1
Enter new value: updated value for key 1
Request Latency: 0.ZZZ ms
HTTP Status: 200
Server Response Body:
{"message":"Key-value pair updated"}

Enter command (add, get, update, delete, exit): delete
Enter key to delete: mykey1
Request Latency: 0.AAA ms
HTTP Status: 200
Server Response Body:
{"message":"Key-value pair deleted"}

Enter command (add, get, update, delete, exit): exit
```