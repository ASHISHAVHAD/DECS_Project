#!/bin/bash

# ==============================================================================
# run_scalability_test.sh
# 
# Automates load testing with specific core pinning and targeted monitoring.
# Runs exactly 10 load levels distributed between 1 and MAX_CLIENTS.
# ==============================================================================

# --- Configuration ---
LOAD_GEN_EXEC="./load_gen"
RESULTS_FILE="performance_results_$(date +%Y-%m-%d_%H-%M-%S).csv"

# Cores where the K/V Server is running (Used for Monitoring)
# NOTE: Server must be manually pinned to these cores (e.g., taskset -c 1,2,9,10 ./server_app)
SERVER_CORES="1,2,9,10"

# Cores where this Load Generator will run (Used for Execution)
LOAD_GEN_CORES="3,4,5,6,7,11,12,13,14,15"

INTERVAL=1
CPU_LOG="cpu_stats.log"
IO_LOG="io_stats.log"
SETTLE_TIME=5

# --- Argument Validation ---
if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <max_clients> <duration_per_run_sec> <workload_type> [other_args...]"
    echo "Example: $0 100 10 0"
    exit 1
fi

MAX_CLIENTS=$1
DURATION_PER_RUN=$2
WORKLOAD_TYPE=$3
WORKLOAD_ARGS="${@:4}"

if ! [ -x "$LOAD_GEN_EXEC" ]; then
    echo "Error: Load generator executable not found at '$LOAD_GEN_EXEC'"
    exit 1
fi

# --- Calculate Load Levels (10 Steps) ---
declare -a CLIENT_LEVELS

if [ "$MAX_CLIENTS" -le 10 ]; then
    # If max is small, just run 1 to MAX (e.g., 1, 2, 3, 4, 5)
    for ((i=1; i<=MAX_CLIENTS; i++)); do CLIENT_LEVELS+=($i); done
else
    # If max is large, calculate 10 linear steps
    # Always start with 1
    CLIENT_LEVELS+=(1)
    
    # Calculate 8 intermediate steps
    # Formula: val = 1 + (step_index * (MAX - 1) / 9)
    for ((i=1; i<9; i++)); do
        val=$(( 1 + (i * (MAX_CLIENTS - 1) / 9) ))
        CLIENT_LEVELS+=($val)
    done
    
    # Always end with MAX
    CLIENT_LEVELS+=($MAX_CLIENTS)
fi

echo "------------------------------------------------------"
echo "Test Plan: ${#CLIENT_LEVELS[@]} Load Levels"
echo "Client Counts: ${CLIENT_LEVELS[*]}"
echo "------------------------------------------------------"


# --- Cleanup Function ---
cleanup() {
    rm -f $CPU_LOG $IO_LOG
    if [ ! -z "$MPSTAT_PID" ]; then pkill -P $MPSTAT_PID &>/dev/null; fi
    if [ ! -z "$IOSTAT_PID" ]; then pkill -P $IOSTAT_PID &>/dev/null; fi
}
trap cleanup EXIT

# --- CSV Header ---
echo "Clients,Throughput(req/s),AvgRespTime(ms),CacheHitRate(%),ServerCPU(%),MaxDiskIO(%)" > "$RESULTS_FILE"

# --- Main Loop ---
for clients in "${CLIENT_LEVELS[@]}"; do
    echo -e "\n--- Running test for $clients client(s) ---"

    # 1. Start Monitoring
    # -P $SERVER_CORES: Monitors only the specific cores
    mpstat -P $SERVER_CORES $INTERVAL $((DURATION_PER_RUN + 2)) > $CPU_LOG & 
    MPSTAT_PID=$!
    
    # -d: Device utilization only, -x: Extended stats
    iostat -d -x $INTERVAL $((DURATION_PER_RUN + 2)) > $IO_LOG &
    IOSTAT_PID=$!
    
    sleep 1

    # 2. Run Load Generator
    # taskset pins the load gen so it doesn't interfere with server cores
    echo "Running load generator on cores $LOAD_GEN_CORES..."
    LOAD_GEN_OUTPUT=$(taskset -c $LOAD_GEN_CORES $LOAD_GEN_EXEC $clients $DURATION_PER_RUN $WORKLOAD_TYPE $WORKLOAD_ARGS)
    
    # Print output to console
    echo "$LOAD_GEN_OUTPUT"
    
    sleep 2
    kill $MPSTAT_PID $IOSTAT_PID 2>/dev/null
    wait $MPSTAT_PID $IOSTAT_PID 2>/dev/null

    # 3. Process Results
    echo "Processing metrics..."

    # Extract Load Gen Metrics
    THROUGHPUT=$(echo "$LOAD_GEN_OUTPUT" | grep "Average Throughput" | awk '{print $3}')
    AVG_RESP_TIME=$(echo "$LOAD_GEN_OUTPUT" | grep "Average Response Time" | awk '{print $4}')
    CACHE_HIT_RATE=$(echo "$LOAD_GEN_OUTPUT" | grep "Cache Hit Rate" | awk '{print $5}')

    # Defaults
    THROUGHPUT=${THROUGHPUT:-0}
    AVG_RESP_TIME=${AVG_RESP_TIME:-0}
    CACHE_HIT_RATE=${CACHE_HIT_RATE:-0}

    # --- CPU Calculation (Specific Cores) ---
    # Parses mpstat for the pinned cores and averages them
    CPU_UTIL=$(awk -v cores="$SERVER_CORES" '
        BEGIN {
            split(cores, c_arr, ",");
            for (c in c_arr) target_cores[c_arr[c]] = 1;
        }
        /^Average:/ && ($2 in target_cores) {
            # 100 - %idle
            used = 100 - $NF;
            sum += used;
            count++;
        }
        END {
            if (count > 0) printf "%.2f", sum / count;
            else print "0.00";
        }
    ' $CPU_LOG)

    # --- I/O Calculation (Max Device) ---
    IO_UTIL=$(awk '
        BEGIN { util_col=0; max_avg=0 }
        /%util/ { 
            for(i=1;i<=NF;i++) if($i=="%util") util_col=i 
        }
        util_col > 0 && $util_col ~ /^[0-9.]/ {
            dev_sum[$1] += $util_col
            dev_count[$1]++
        }
        END {
            for (d in dev_sum) {
                if (dev_count[d] > 0) {
                    avg = dev_sum[d] / dev_count[d]
                    if (avg > max_avg) max_avg = avg
                }
            }
            printf "%.2f", max_avg
        }
    ' $IO_LOG)

    if [ -z "$IO_UTIL" ]; then IO_UTIL="0.00"; fi

    # 4. Save Results
    echo "$clients,$THROUGHPUT,$AVG_RESP_TIME,$CACHE_HIT_RATE,$CPU_UTIL,$IO_UTIL" >> "$RESULTS_FILE"
    echo "Stats Saved: Server CPU: ${CPU_UTIL}%, Max Disk IO: ${IO_UTIL}%"

    # Wait before next level (only if not the last one)
    if [ "$clients" -ne "$MAX_CLIENTS" ]; then 
        echo "Waiting $SETTLE_TIME seconds to cool down..."
        sleep $SETTLE_TIME
    fi
done

echo "======================================================"
echo "Test Complete. Results saved to $RESULTS_FILE"
echo "======================================================"