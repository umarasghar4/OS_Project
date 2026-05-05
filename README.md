# IoT Pipeline — Project Overview

This repository implements a small POSIX-based data ingestion and processing pipeline for IoT-style CSV data. It demonstrates inter-process communication (named pipes and POSIX shared memory), multithreading with a bounded queue, and safe shutdown signaling.

This README consolidates the project commands, data flow, and component explanations into a single, human-friendly reference. It also documents a small simplification made to the `ingester` (reads now use the `MAX_CHUNK_SIZE` constant).

## Quick Start

- Prepare folders (run from repository root):
  ```bash
  mkdir -p data logs

- Make the orchestrator executable:

    chmod +x run.sh

- Run the full pipeline (builds first):

    ./run.sh -n 4
    
- To remove compiled binaries, logs and reports:
  make clean

## What this project does (high level)

1. `run.sh` compiles the code and launches `dispatcher`.
2. `dispatcher` creates a FIFO and shared memory, then forks three children:
   - `ingester` — reads CSV and writes fixed-size binary chunks into the FIFO
   - `processor` — reads chunks, stitches partial rows, dispatches complete rows to worker threads
   - `reporter` — waits for the processor, then reads shared memory and writes final reports
3. The `ingester` sends a final EOF chunk (chunk_id = -1) to signal shutdown.
4. `processor` aggregates statistics into shared memory and posts a named semaphore to wake `reporter`.
5. `reporter` writes `report.csv` and `report.txt`, signals the parent, and exits.

Detailed flow tracing is available in `flow.txt`.

## Files & Roles

- `common.h` — shared constants and the IPC contracts (`ChunkHeader`, `SharedData`).
- `dispatcher.c` — creates IPC resources, spawns the three worker processes, and handles cleanup and signals.
- `ingester.c` — reads CSV data and writes binary chunks (header + bytes) to the FIFO.
- `processor.c` — reads chunks, stitches fragmented lines, enqueues complete rows for worker threads, aggregates results into shared memory.
- `reporter.c` — waits for the processor to finish, reads shared memory, and writes `report.csv` and `report.txt`.
- `run.sh` — orchestrator script to build and run the full pipeline.
- `Makefile` — build rules for all components.

## Build & Manual Testing

- Build everything with `make`.
- Manual integration test using a FIFO:


    mkfifo /tmp/test_pipe
    ./processor /tmp/test_pipe /test_shm 4 100 &
    ./ingester /tmp/test_pipe data
    
    Then run the reporter after the processor finished:

    ./reporter /test_shm .

Clean up:

    rm /tmp/test_pipe
    make clean

## Design Notes & Simplifications

- The project uses a small fixed-size bounded queue with `sem_empty`/`sem_full` and a `pthread_mutex_t` protecting the ring buffer.
- Worker threads parse lines using `strtok_r` and update the aggregation table under a mutex.
- Small simplification applied: `ingester.c` now reads up to the project constant `MAX_CHUNK_SIZE` (from `common.h`) instead of a hard-coded value. This removes a magic number and makes the behavior consistent with the shared constants.

## Troubleshooting & Common Commands

- If `run.sh` reports "No CSV files found", confirm `data/sample.csv` exists.
- If the pipeline hangs, check `logs/` files for component output; the `ingester` does a blocking `open()` on the FIFO until a reader appears.

## Build targets

- `make` — builds all binaries.
- `make clean` — removes binaries and artifacts.

## License & Contact

This is a learning/demo project; adapt freely. If you want changes or deeper refactors (thread-safe queue rewrites, better aggregation bookkeeping, or unit tests), tell me which component to focus on next.
