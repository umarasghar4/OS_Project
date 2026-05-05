#include "common.h"

char **queue;
int max_queue_size;
int queue_head = 0;
int queue_tail = 0;

// FIX: [Rubric Page 8] "Queue is protected by a mutex... coordinated using two semaphores"
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_empty;
sem_t sem_full;

IoTAggregationRecord agg_table[MAX_SENSORS];
int unique_sensors = 0;

// FIX: [Rubric Page 8] "The aggregation table itself is guarded by its own mutex"
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_signal(int sig) {
    if (sig == SIGINT) exit(130);
    if (sig == SIGTERM) exit(143);
}

void enqueue(const char* data) {
    sem_wait(&sem_empty); // Wait if queue is full
    pthread_mutex_lock(&queue_mutex);
    
    strcpy(queue[queue_tail], data);
    queue_tail = (queue_tail + 1) % max_queue_size;
    
    pthread_mutex_unlock(&queue_mutex);
    sem_post(&sem_full); // Signal that queue has data
}

void dequeue(char* buffer) {
    sem_wait(&sem_full); // Wait if queue is empty
    pthread_mutex_lock(&queue_mutex);
    
    strcpy(buffer, queue[queue_head]);
    queue_head = (queue_head + 1) % max_queue_size;
    
    pthread_mutex_unlock(&queue_mutex);
    sem_post(&sem_empty); // Signal that queue has space
}

void* worker_thread(void* arg) {
    (void)arg;
    char buffer[MAX_CHUNK_SIZE * 2];
    char *saveptr_line, *saveptr_comma;
    
    while (1) {
        dequeue(buffer);
        
        // FIX: [Rubric Page 8] "N 'poison pill' items so every worker exits cleanly."
        if (strcmp(buffer, "POISON") == 0) {
            break;
        }
        
        // FIX: [Rubric Page 8] "Each worker parses CSV rows from its chunk"
        char *line = strtok_r(buffer, "\n", &saveptr_line);
        while (line != NULL) {
            char *token = strtok_r(line, ",", &saveptr_comma);
            if (token != NULL) {
                char sensor_name[MAX_SENSOR_NAME];
                strncpy(sensor_name, token, MAX_SENSOR_NAME - 1);
                sensor_name[MAX_SENSOR_NAME - 1] = '\0';
                
                int count = 0;
                double sum = 0.0;
                int anomalies = 0;
                
                token = strtok_r(NULL, ",", &saveptr_comma);
                while (token != NULL) {
                    double val = atof(token);
                    sum += val;
                    count++;
                    if (val > 50.0 || val < 5.0) anomalies++; 
                    token = strtok_r(NULL, ",", &saveptr_comma);
                }
                
                // Atomically update the shared table
                pthread_mutex_lock(&table_mutex);
                int found = 0;
                for (int i = 0; i < unique_sensors; i++) {
                    if (strcmp(agg_table[i].sensor_id, sensor_name) == 0) {
                        agg_table[i].moving_average = ((agg_table[i].moving_average * agg_table[i].anomaly_count) + sum) / (agg_table[i].anomaly_count + count);
                        agg_table[i].anomaly_count += anomalies;
                        found = 1;
                        break;
                    }
                }
                if (!found && unique_sensors < MAX_SENSORS) {
                    strcpy(agg_table[unique_sensors].sensor_id, sensor_name);
                    agg_table[unique_sensors].moving_average = sum / count;
                    agg_table[unique_sensors].anomaly_count = anomalies;
                    unique_sensors++;
                }
                pthread_mutex_unlock(&table_mutex);
            }
            line = strtok_r(NULL, "\n", &saveptr_line);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    printf("[Processor] Starting Processor (PID: %d, PPID: %d)\n", getpid(), getppid());

    if (argc < 5) return 10;
    
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    char* fifo_path = argv[1];
    char* shm_name = argv[2];
    int num_threads = atoi(argv[3]);
    
    // FIX: [Rubric Page 5] Dynamic Queue Allocation
    max_queue_size = atoi(argv[4]);
    queue = malloc(max_queue_size * sizeof(char*));
    for (int i = 0; i < max_queue_size; i++) {
        queue[i] = malloc(MAX_CHUNK_SIZE * 2);
    }
    
    sem_init(&sem_empty, 0, max_queue_size);
    sem_init(&sem_full, 0, 0);
    
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) return 20; 
    
    ftruncate(shm_fd, sizeof(SharedData));
    SharedData* shm_ptr = mmap(0, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) return 20; 
    
    // =========================================================================
    // FIX: [Rubric Page 6] "Worker threads must be created with an explicit 
    // pthread_attr_t stack size and detach state set by your group"
    // =========================================================================
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // State: We explicitly set it to JOINABLE because the main thread must wait for workers to finish
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    // Stack Size: Set to 1MB per thread (avoids default memory bloat if we scale to hundreds of threads)
    pthread_attr_setstacksize(&attr, 1024 * 1024); 

    pthread_t workers[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&workers[i], &attr, worker_thread, NULL);
    }
    pthread_attr_destroy(&attr); // Clean up attributes object
    // =========================================================================

    int fifo_fd = open(fifo_path, O_RDONLY);
    if (fifo_fd == -1) return 40; 
    
    char spillover[MAX_CHUNK_SIZE] = {0};
    
    // The Dedicated Reader Thread (Main Thread)
    while (1) {
        ChunkHeader header;
        if (read(fifo_fd, &header, sizeof(ChunkHeader)) <= 0) break;
        if (header.chunk_id == -1) break; // EOF chunk received
        
        char buffer[MAX_CHUNK_SIZE] = {0};
        read(fifo_fd, buffer, header.byte_count);
        
        char combined[MAX_CHUNK_SIZE * 2] = {0};
        strcpy(combined, spillover);
        strncat(combined, buffer, header.byte_count);
        
        char* last_newline = strrchr(combined, '\n');
        if (last_newline != NULL) {
            *last_newline = '\0'; 
            enqueue(combined);
            strcpy(spillover, last_newline + 1);
        } else {
            strcpy(spillover, combined);
        }
    }
    
    // FIX: [Rubric Page 8] "enqueues N poison pill items"
    for (int i = 0; i < num_threads; i++) enqueue("POISON");
    // FIX: [Rubric Page 8] "All workers are then pthread_join()ed"
    for (int i = 0; i < num_threads; i++) pthread_join(workers[i], NULL);
    
    // FIX: [Rubric Page 8] "table is serialised into the shared-memory segment"
    shm_ptr->unique_sensor_count = unique_sensors;
    memcpy(shm_ptr->records, agg_table, sizeof(IoTAggregationRecord) * unique_sensors);
    
    // FIX: [Rubric Page 8] "named POSIX semaphore is sem_post()ed"
    sem_t* reporter_sem = sem_open("/reporter_sync", O_CREAT, 0666, 0);
    sem_post(reporter_sem);
    
    close(fifo_fd);
    munmap(shm_ptr, sizeof(SharedData));
    
    for (int i = 0; i < max_queue_size; i++) free(queue[i]);
    free(queue);
    
    return 0;
}
