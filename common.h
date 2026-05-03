#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>

#define MAX_CHUNK_SIZE 1024   // Max bytes of CSV text per chunk
#define MAX_SENSORS 50        // Max unique IoT devices we expect
#define MAX_SENSOR_NAME 32    // Max characters in a sensor's name (e.g., "Temp_LivingRoom")

typedef struct {
    int chunk_id;
    size_t byte_count;    
    int source_file_id;
} ChunkHeader;


typedef struct {
    char sensor_id[MAX_SENSOR_NAME];
    double moving_average;
    int anomaly_count;
} IoTAggregationRecord;


typedef struct {
    int unique_sensor_count;                     // How many sensors we actually found
    IoTAggregationRecord records[MAX_SENSORS]; 
} SharedData;

#endif