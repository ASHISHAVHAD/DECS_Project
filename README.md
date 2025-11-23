**Roll No: 25M2115**

**Name: Ashish Balasaheb Avhad**

**Subject: CS744 Design and Engineering of Computer Systems**

**Instructor: Prof. Mythili Vutukuru**

GitHub Repository Link: https://github.com/ASHISHAVHAD/DECS_Project

**Project Title:** Performance testing and benchmarking of Key-Value Server and identification of bottlenecks.

**Description:** This project aims to implement and test a functionally correct server, test its performance across various loads and identify software and hardware bottlenecks. The server is a key-value store where users can send a key value to store, access value by providing a key, update value associated with a key, and delete a key-value pair. The server works over HTTP. The persistent data is stored in a MySQL server. An in-memory LRU cache is implemented to store recently accessed key-value pairs for fast access. For concurrency, a thread pool mechanism is used, where each thread looks for a request in the queue and picks up available requests to process.

Additionally, a custom **Load Generator** is included to stress-test the server using various synthetic workloads (Write-Heavy, Read-Heavy, Zipfian distribution) and measure throughput, latency, and cache hit rates.

**Technical Details:** Server and Load Generator are implemented in C++. For HTTP operations, `httplib` library is used. For storing data, MySQL server is used. For database connection, `libmysqlcppconn-dev` is used.

**Directory Structure:**

```text
    |- interactive_client
        |- main.cpp
        |- Makefile

    |- load_generator
        |- main.cpp
        |- config.h
        |- Makefile
        |- run_and_monitor.sh

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
```

**Steps to setup and run the project (Linux-only):**

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

7. Compile load generator code:

    ```bash
    cd ../load_generator
    make
    chmod +x run_and_monitor.sh
    ```
    This will create an executable named `load_gen` and make the wrapper script executable.

---

### Running the Application

**1. Start the Server**
Open a terminal window and change current working directory to `DECS_Project/server`:

```bash
./kv_server
```

**2. Run Interactive Client (Functional Testing)**
Open a new terminal and change current working directory to `DECS_Project/interactive_client`:

```bash
./interactive_client
```

**3. Run Load Generator (Performance Testing)**
The load generator allows you to stress test the server. Open a new terminal and change directory to `DECS_Project/load_generator`.

Usage:
```bash
./run_and_monitor.sh <max_clients> <duration_per_run_sec> <workload_type> [other_args...]
```

*   **max_clients**: The script will iterate from 1 client up to this number (or in steps).
*   **duration_per_run_sec**: How long each test level runs.
*   **workload_type**:
    *   `0` : PUT_ALL (Write-Heavy)
    *   `1` : GET_ALL (Read-Heavy, Random Keys/Cache Miss)
    *   `2` : GET_POPULAR (Read-Heavy, Hot Keys/Cache Hit)
    *   `3` : GET_PUT (Mixed)

Example:
```bash
# Run a test scaling up to 100 clients, 30 seconds per run, using Workload 2 (GET_POPULAR)
./run_and_monitor.sh 100 30 2
```
*Results will be saved to a CSV file in the current directory.*

---

### CPU Pinning (Taskset)

To simulate a realistic scenario and ensure consistent benchmarking, you should pin the Server, Client, and Database to different CPU cores.

**1. Pinning MySQL Server:**

```bash
sudo systemctl edit mysql.service
```
Add the following to the file (replace `<list of cpu cores>` with actual core numbers, e.g., `0,8`):
```bash
[Service]
CPUAffinity=<list of cpu cores>
```
Reload and restart:
```bash
sudo systemctl daemon-reload
sudo systemctl restart mysql
```
Verify pinning:
```bash
MYSQL_PID=$(pgrep mysqld | head -n 1)
taskset -cp $MYSQL_PID
```

**2. Pinning the Key-Value Server:**
Assuming you want to run the server on cores 1, 2, 9, and 10:
```bash
taskset -c 1,2,9,10 ./kv_server
```

**3. Pinning the Load Generator:**
The `run_and_monitor.sh` script usually handles pinning for the load generator internally (check variables in the script), or you can pin the script execution itself:
```bash
taskset -c 3-7,11-15 ./run_and_monitor.sh 100 30 2
```

**4. Pinning the Interactive Client:**
```bash
taskset -c 3 ./interactive_client
```

---

### Sample Interactive Client Output

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