#!/bin/bash

INPUT_DIR="data"
OUTPUT_DIR="."
THREADS=4
PID_FILE=".dispatcher.pid"

usage() {
    echo "Usage: $0 [-i input_dir] [-o output_dir] [-n threads] [-c] [-h]"
    exit 10
}

cleanup() {
    if [ -f "$PID_FILE" ]; then
        DISPATCHER_PID=$(cat "$PID_FILE")
        if kill -0 "$DISPATCHER_PID" 2>/dev/null; then
            kill -TERM "$DISPATCHER_PID"
        fi
        rm -f "$PID_FILE"
    fi
}

print_summary() {
    local end_time=$1
    local start_time=$2
    local duration=$((end_time - start_time))
    
    local records=0
    if [ -f "${OUTPUT_DIR}/report.csv" ]; then
        records=$(wc -l < "${OUTPUT_DIR}/report.csv")
        records=$((records - 1))
    fi
    
    echo -e "\n====================================="
    echo "PIPELINE SUMMARY"
    echo "Total Runtime: ${duration} seconds"
    echo "Records Processed: ${records}"
    echo "====================================="
}

trap cleanup EXIT INT TERM

while getopts "i:o:n:ch" opt; do
    case ${opt} in
        i ) INPUT_DIR=$OPTARG ;;
        o ) OUTPUT_DIR=$OPTARG ;;
        n ) THREADS=$OPTARG ;;
        c ) make clean; exit 0 ;;
        h ) usage ;;
        * ) usage ;;
    esac
done

command -v gcc >/dev/null 2>&1 || { echo "gcc not installed."; exit 10; }
command -v make >/dev/null 2>&1 || { echo "make not installed."; exit 10; }

csv_found=0
for file in "$INPUT_DIR"/*.csv; do
    if [ -f "$file" ]; then
        csv_found=1
        break
    fi
done

if [ $csv_found -eq 0 ]; then
    echo "Error: No CSV files found in $INPUT_DIR"
    exit 40
fi

make || { echo "Build failed."; exit 10; }

START_TIME=$(date +%s)

./dispatcher "$INPUT_DIR" "$OUTPUT_DIR" "$THREADS" "/tmp/master_pipe" "/master_shm" &
DISPATCHER_PID=$!
echo $DISPATCHER_PID > "$PID_FILE"

wait $DISPATCHER_PID
EXIT_STATUS=$?

END_TIME=$(date +%s)
print_summary "$END_TIME" "$START_TIME"

exit $EXIT_STATUS