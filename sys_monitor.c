// sys_monitor.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CPU 256
#define LINE_SZ 512

/* Lectura de /proc/meminfo */
void read_meminfo(unsigned long long *mem_total_kb,
                  unsigned long long *mem_avail_kb,
                  unsigned long long *swap_total_kb,
                  unsigned long long *swap_free_kb) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { perror("fopen /proc/meminfo"); return; }

    char line[LINE_SZ];
    *mem_total_kb = *mem_avail_kb = *swap_total_kb = *swap_free_kb = 0ULL;

    while (fgets(line, sizeof(line), f)) {
        unsigned long long val;
        if (sscanf(line, "MemTotal: %llu kB", &val) == 1) *mem_total_kb = val;
        else if (sscanf(line, "MemAvailable: %llu kB", &val) == 1) *mem_avail_kb = val;
        else if (sscanf(line, "SwapTotal: %llu kB", &val) == 1) *swap_total_kb = val;
        else if (sscanf(line, "SwapFree: %llu kB", &val) == 1) *swap_free_kb = val;
    }
    fclose(f);
}

/* Lectura de /proc/cpuinfo para modelo y conteo */
void read_cpuinfo(char *model_buf, size_t buf_sz, int *num_cpus) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) { perror("fopen /proc/cpuinfo"); return; }

    char line[LINE_SZ];
    model_buf[0] = '\0';
    *num_cpus = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0 && model_buf[0] == '\0') {
            char *p = strchr(line, ':');
            if (p) {
                ++p;
                while (*p == ' ' || *p == '\t') ++p;
                strncpy(model_buf, p, buf_sz - 1);
                /* quitar newline */
                char *nl = strchr(model_buf, '\n');
                if (nl) *nl = '\0';
            }
        }
        if (strncmp(line, "processor", 9) == 0) (*num_cpus)++;
    }
    fclose(f);
    if (*num_cpus == 0) *num_cpus = 1; // fallback
}

/* Lee líneas cpuN de /proc/stat y llena arrays total/idle por cpu */
int read_proc_stat(unsigned long long total[], unsigned long long idle[], int max_cpu) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) { perror("fopen /proc/stat"); return -1; }

    char line[LINE_SZ];
    int idx = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu", 3) != 0) break;
        if (strncmp(line, "cpu ", 4) == 0) continue; // salto la línea agregada "cpu " global

        /* tokenizar la línea */
        char *saveptr;
        char *tk = strtok_r(line, " ", &saveptr); // "cpuN"
        if (!tk) continue;

        unsigned long long vals[16];
        int vcount = 0;
        while ((tk = strtok_r(NULL, " ", &saveptr)) != NULL && vcount < 16) {
            if (tk[0] == '\n' || tk[0] == '\0') continue;
            /* quitar posible newline y convertir */
            char *nl = strchr(tk, '\n');
            if (nl) *nl = '\0';
            vals[vcount++] = strtoull(tk, NULL, 10);
        }
        unsigned long long tot = 0;
        for (int i = 0; i < vcount; ++i) tot += vals[i];
        unsigned long long idl = 0;
        if (vcount > 3) idl = vals[3];          // idle
        if (vcount > 4) idl += vals[4];         // iowait

        if (idx < max_cpu) {
            total[idx] = tot;
            idle[idx]  = idl;
            idx++;
        } else break;
    }
    fclose(f);
    return idx;
}

int main(void) {
    char cpu_model[256];
    int num_cpus = 0;
    read_cpuinfo(cpu_model, sizeof(cpu_model), &num_cpus);

    unsigned long long prev_total[MAX_CPU] = {0}, prev_idle[MAX_CPU] = {0};
    unsigned long long cur_total[MAX_CPU], cur_idle[MAX_CPU];

    /* Inicializar lectura previa para tener una base */
    int detected = read_proc_stat(prev_total, prev_idle, MAX_CPU);
    if (detected > 0) num_cpus = detected;

    while (1) {
        /* Sleep 2 segundos antes de tomar la muestra actual */
        sleep(2);

        /* Leer memoria */
        unsigned long long mem_total_kb, mem_avail_kb, swap_total_kb, swap_free_kb;
        read_meminfo(&mem_total_kb, &mem_avail_kb, &swap_total_kb, &swap_free_kb);

        /* Leer stat actual */
        int cur_count = read_proc_stat(cur_total, cur_idle, MAX_CPU);

        /* Limpiar pantalla (ANSI) */
        printf("\033[H\033[J");
        printf("=== Sys Monitor (simple) ===\n");
        printf("Procesador: %s\n", cpu_model);
        printf("Núcleos detectados: %d\n\n", cur_count);

        /* Memoria en MB */
        printf("Memoria total: %llu MB\n", mem_total_kb / 1024);
        printf("Memoria disponible (approx): %llu MB\n", mem_avail_kb / 1024);
        printf("Memoria usada: %llu MB\n", (mem_total_kb - mem_avail_kb) / 1024);
        printf("Swap total: %llu MB, Swap usado: %llu MB\n\n",
               swap_total_kb / 1024, (swap_total_kb - swap_free_kb) / 1024);

        /* Carga por núcleo */
        for (int i = 0; i < cur_count && i < MAX_CPU; ++i) {
            unsigned long long totald = (cur_total[i] >= prev_total[i]) ? (cur_total[i] - prev_total[i]) : 0;
            unsigned long long idled  = (cur_idle[i]  >= prev_idle[i])  ? (cur_idle[i]  - prev_idle[i])  : 0;
            double usage = 0.0;
            if (totald > 0) usage = (1.0 - ((double)idled / (double)totald)) * 100.0;
            printf("CPU%-2d: %6.2f %%\n", i, usage);

            /* actualizar prev para próxima iteración */
            prev_total[i] = cur_total[i];
            prev_idle[i]  = cur_idle[i];
        }

        printf("\n(Actualiza cada 2s)  Presiona Ctrl+C para salir.\n");
        fflush(stdout);
    }
    return 0;
}
