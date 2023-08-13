#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define INTERVAL 1  
#define NUM_CPU_CORES sysconf(_SC_NPROCESSORS_ONLN)

typedef struct {
    unsigned long long user, prevuser;
    unsigned long long nice, prevnice;
    unsigned long long system, prevsystem;
    unsigned long long idle, previdle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
    double CPU_Usage;
} CPUStats;

pthread_mutex_t cpu_stats_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cpu_stats_updated = PTHREAD_COND_INITIALIZER; 


void* Reader(void* arg) {
    CPUStats* cpu_stats = (CPUStats*)arg;

    while (1) {
        FILE* file = fopen("/proc/stat", "r");
        if (!file) {
            fprintf(stderr, "Error opening /proc/stat file.\n");
            exit(EXIT_FAILURE);
        }

        char buffer[256];
        int cpu_id = 0;

        while (fgets(buffer, sizeof(buffer), file) && cpu_id <= NUM_CPU_CORES) {
            if (strncmp(buffer, "cpu", 3) == 0) {   //if there is "cpu" in the line
                sscanf(buffer + 5, "%llu %llu %llu %llu %llu %llu %llu %llu",
                       &cpu_stats[cpu_id].user, &cpu_stats[cpu_id].nice,
                       &cpu_stats[cpu_id].system, &cpu_stats[cpu_id].idle,
                       &cpu_stats[cpu_id].iowait, &cpu_stats[cpu_id].irq,
                       &cpu_stats[cpu_id].softirq, &cpu_stats[cpu_id].steal);  //+5 to eliminate "cpu_..."
                cpu_id++;
            }
        }

        fclose(file);

        pthread_cond_signal(&cpu_stats_updated);

        sleep(INTERVAL); 
    }

    return NULL;
}

void* Analyzer(void* arg) {
    CPUStats* cpu_stats = (CPUStats*)arg;
    CPUStats* prev_cpu_stats = (CPUStats*)malloc(sizeof(CPUStats) * (NUM_CPU_CORES+1));
    if (!prev_cpu_stats) {
        fprintf(stderr, "Error allocating memory for prev_cpu_stats.\n");
        exit(EXIT_FAILURE);
    }
    

    
    for (int core_id = 0; core_id <= NUM_CPU_CORES; core_id++) {
        prev_cpu_stats[core_id] = cpu_stats[core_id];
    }
    
    while (1) {
        pthread_mutex_lock(&cpu_stats_mutex);

        
        pthread_cond_wait(&cpu_stats_updated, &cpu_stats_mutex);


        
        for (int core_id = 1; core_id <= NUM_CPU_CORES; core_id++) {
      

            unsigned long long PrevIdle = prev_cpu_stats[core_id].idle - prev_cpu_stats[core_id].iowait;

            unsigned long long Idle = cpu_stats[core_id].idle - cpu_stats[core_id].iowait;

            unsigned long long PrevNonIdle = prev_cpu_stats[core_id].user + prev_cpu_stats[core_id].nice +
                                                prev_cpu_stats[core_id].system + prev_cpu_stats[core_id].irq +
                                                prev_cpu_stats[core_id].softirq + prev_cpu_stats[core_id].steal;

            unsigned long long NonIdle = cpu_stats[core_id].user + cpu_stats[core_id].nice +
                                                cpu_stats[core_id].system + cpu_stats[core_id].irq +
                                                cpu_stats[core_id].softirq + cpu_stats[core_id].steal;     

            unsigned long long PrevTotal = PrevIdle + PrevNonIdle;

            unsigned long long Total = Idle + NonIdle;

            unsigned long long totald = Total - PrevTotal;

            unsigned long long idled = Idle - PrevIdle;


            cpu_stats[core_id].CPU_Usage = ((double)(totald - idled) / totald) * 100.0;


            prev_cpu_stats[core_id] = cpu_stats[core_id];
            pthread_cond_signal(&cpu_stats_updated);
        }

        pthread_mutex_unlock(&cpu_stats_mutex);
        
    }

    free(prev_cpu_stats);
    return NULL;
}


void* Printer(void* arg) {
    CPUStats* cpu_stats = (CPUStats*)arg;
  
   

    while (1) {
        sleep(INTERVAL); 
        pthread_mutex_lock(&cpu_stats_mutex);
    
        for (int core_id = 1; core_id <= NUM_CPU_CORES; core_id++) {
            printf("CPU Usage for core #%d: %.2f%%\n", core_id, cpu_stats[core_id].CPU_Usage);
                
        }
    
        pthread_mutex_unlock(&cpu_stats_mutex);
       
    }


    return NULL;
}

int main() {
    long num_cores = NUM_CPU_CORES;

    if (num_cores <= 0) {
        printf("Failed to determine the number of CPU cores.\n");
        return 1;
    }

    
    CPUStats* cpu_stats = (CPUStats*)malloc(sizeof(CPUStats) * (num_cores+1));
    if (cpu_stats == NULL) {
        fprintf(stderr, "Error allocating memory for cpu_stats.\n");
        return 1;
    }


    pthread_t reader_thread, analyzer_thread, printer_thread;

 
    if (pthread_create(&reader_thread, NULL, Reader, cpu_stats) != 0) {
        fprintf(stderr, "Error creating thread.\n");
        return 1;
    }

   
    if (pthread_create(&analyzer_thread, NULL, Analyzer, cpu_stats) != 0) {
        fprintf(stderr, "Error creating thread.\n");
        return 1;
    }

    if (pthread_create(&printer_thread, NULL, Printer, cpu_stats) != 0) {
        fprintf(stderr, "Error creating printer thread.\n");
        free(cpu_stats);
        return 1;
    }

  
    pthread_join(reader_thread, NULL);
    pthread_join(analyzer_thread, NULL);
    pthread_join(printer_thread, NULL); 
    
    return 0;
}
