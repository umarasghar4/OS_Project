#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include "common.h" 

volatile sig_atomic_t keep_running = 1;
int files_processed = 0;
int chunks_sent = 0;
int total_bytes_sent = 0;

void handle_signal(int sig) {
    if (sig == SIGTERM) {
        keep_running = 0;
    } else if (sig == SIGUSR1) {
        fprintf(stderr, "[Ingester PID: %d, PPID: %d] Stats - Files: %d, Chunks: %d, Bytes: %d\n",
                getpid(), getppid(), files_processed, chunks_sent, total_bytes_sent);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <fifo_path> <input_csv>\n", argv[0]);
        return 10;
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    int fifo_fd = open(argv[1], O_WRONLY);
    if (fifo_fd == -1) {
        perror("Ingester: Failed to open FIFO");
        return 20;
    }

    int csv_fd = open(argv[2], O_RDONLY);
    if (csv_fd == -1) {
        perror("Ingester: Failed to open CSV");
        close(fifo_fd);
        return 40;
    }

    char buffer[MAX_CHUNK_SIZE];
    ssize_t bytes_read;
    int current_chunk_id = 1;

    while (keep_running && (bytes_read = read(csv_fd, buffer, MAX_CHUNK_SIZE)) > 0) {
        ChunkHeader header;
        header.chunk_id = current_chunk_id++;
        header.byte_count = bytes_read;
        header.source_file_id = 1; 

        write(fifo_fd, &header, sizeof(ChunkHeader));
        write(fifo_fd, buffer, bytes_read);

        chunks_sent++;
        total_bytes_sent += bytes_read;
    }

    ChunkHeader eof_header = {-1, 0, 1};
    write(fifo_fd, &eof_header, sizeof(ChunkHeader));

    files_processed++;

    close(csv_fd);
    close(fifo_fd);
    
    return 0;
}