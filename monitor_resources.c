/*
 * monitor_resources.c
 * CC3064 Sistemas Operativos - Laboratorio #5
 * Programa 2: Regulacion de recursos con Monitor (mutex + cond variable)
 *
 * Mejora el Programa 1 al permitir consumir/devolver multiples recursos
 * a la vez. El monitor se implementa con pthread_mutex_t + pthread_cond_t
 * (Silberschatz, cap. 6).
 *
 * Compilacion:
 *   gcc -o monitor_resources monitor_resources.c -lpthread
 * Ejecucion:
 *   ./monitor_resources
 *
 * Genera: bitacora_monitor.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ============================================================
 *  PARAMETROS AJUSTABLES
 * ============================================================ */
#define NUM_THREADS     10   /* Numero de threads a crear                 */
#define NUM_ITERATIONS   5   /* Iteraciones que ejecuta cada thread       */
#define NUM_RESOURCES   10   /* Recursos digitales disponibles            */
#define MAX_CONSUME      3   /* Maximo de recursos que un thread consume  */
#define MAX_SLEEP_MS   500   /* Maximo tiempo de uso del recurso (ms)     */
#define MIN_SLEEP_MS    50   /* Minimo tiempo de uso del recurso (ms)     */
#define LOG_FILE        "bitacora_monitor.txt"

/* ============================================================
 *  MONITOR
 *  Solo decrease_count / increase_count tocan available_resources.
 *  Cualquier acceso fuera de estas funciones rompe la invariante.
 * ============================================================ */
static int             available_resources = NUM_RESOURCES;
static pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  resource_cond  = PTHREAD_COND_INITIALIZER;

/* Mutex independiente para el log (no mezclar con el monitor) */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE           *log_fp;

/* ------------------------------------------------------------
 * log_msg: escribe en stdout y bitacora de forma atomica.
 * ------------------------------------------------------------ */
static void log_msg(const char *msg) {
    pthread_mutex_lock(&log_mutex);
    fprintf(log_fp, "%s\n", msg);
    fflush(log_fp);
    printf("%s\n", msg);
    pthread_mutex_unlock(&log_mutex);
}

/* ------------------------------------------------------------
 * decrease_count: ENTRADA AL MONITOR para consumir recursos.
 *
 * Adquiere monitor_mutex. Si los recursos disponibles son
 * insuficientes, libera el mutex y suspende el thread mediante
 * pthread_cond_wait (patron "espera con condicion").
 * Al despertar revalida la condicion (while, no if).
 * Retorna 0 al consumir exitosamente.
 * ------------------------------------------------------------ */
static int decrease_count(long tid, int iter, int count) {
    char buf[512];

    pthread_mutex_lock(&monitor_mutex);   /* -- Entrada al monitor -- */

    snprintf(buf, sizeof(buf),
             "Thread %ld | Iter %d - Iniciando decrease_count(%d) | "
             "Disponibles: %d/%d",
             tid, iter, count, available_resources, NUM_RESOURCES);
    log_msg(buf);

    /* Esperar en bucle mientras no haya suficientes recursos */
    while (available_resources < count) {
        snprintf(buf, sizeof(buf),
                 "Thread %ld | Iter %d - Recursos insuficientes (%d < %d). "
                 "Esperando en variable de condicion...",
                 tid, iter, available_resources, count);
        log_msg(buf);

        /*
         * pthread_cond_wait:
         *   1. Libera monitor_mutex atomicamente.
         *   2. Suspende el thread.
         *   3. Al ser despertado, reAdquiere monitor_mutex antes de retornar.
         */
        pthread_cond_wait(&resource_cond, &monitor_mutex);

        snprintf(buf, sizeof(buf),
                 "Thread %ld | Iter %d - Despertado. Verificando recursos "
                 "(%d disponibles)...",
                 tid, iter, available_resources);
        log_msg(buf);
    }

    /* Consumir recursos */
    available_resources -= count;

    snprintf(buf, sizeof(buf),
             "Thread %ld | Iter %d - (+) %d recurso(s) TOMADO(S). "
             "Disponibles: %d/%d",
             tid, iter, count, available_resources, NUM_RESOURCES);
    log_msg(buf);

    snprintf(buf, sizeof(buf),
             "Thread %ld | Iter %d - Terminando decrease_count",
             tid, iter);
    log_msg(buf);

    pthread_mutex_unlock(&monitor_mutex); /* -- Salida del monitor -- */
    return 0;
}

/* ------------------------------------------------------------
 * increase_count: ENTRADA AL MONITOR para devolver recursos.
 *
 * Devuelve 'count' recursos y despierta a TODOS los threads
 * en espera (broadcast) para que reevaluen su condicion.
 * Se usa broadcast en lugar de signal porque distintos threads
 * esperan cantidades distintas de recursos.
 * ------------------------------------------------------------ */
static int increase_count(long tid, int iter, int count) {
    char buf[512];

    pthread_mutex_lock(&monitor_mutex);   /* -- Entrada al monitor -- */

    available_resources += count;

    snprintf(buf, sizeof(buf),
             "Thread %ld | Iter %d - (-) %d recurso(s) DEVUELTO(S). "
             "Disponibles: %d/%d",
             tid, iter, count, available_resources, NUM_RESOURCES);
    log_msg(buf);

    /* Notificar a todos los threads en espera */
    pthread_cond_broadcast(&resource_cond);

    pthread_mutex_unlock(&monitor_mutex); /* -- Salida del monitor -- */
    return 0;
}

/* ------------------------------------------------------------
 * thread_func: rutina ejecutada por cada thread.
 * Cada iteracion consume una cantidad aleatoria de recursos,
 * simula trabajo y los devuelve.
 * ------------------------------------------------------------ */
static void *thread_func(void *arg) {
    long tid = (long)arg;
    char buf[256];

    snprintf(buf, sizeof(buf), "Iniciando thread %ld", tid);
    log_msg(buf);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        /* Cantidad aleatoria de recursos a consumir (1..MAX_CONSUME) */
        int count = (rand() % MAX_CONSUME) + 1;

        snprintf(buf, sizeof(buf),
                 "Thread %ld | Iter %d - Se consumiran %d recursos",
                 tid, i + 1, count);
        log_msg(buf);

        decrease_count(tid, i + 1, count);   /* consumir */

        /* Simular trabajo */
        int sleep_ms = (rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1)) + MIN_SLEEP_MS;
        usleep((useconds_t)sleep_ms * 1000);

        snprintf(buf, sizeof(buf),
                 "Thread %ld | Iter %d - Trabajo terminado. "
                 "Devolviendo %d recurso(s).",
                 tid, i + 1, count);
        log_msg(buf);

        increase_count(tid, i + 1, count);   /* devolver */
    }

    snprintf(buf, sizeof(buf),
             "Thread %ld - Terminado (todas las iteraciones completas)", tid);
    log_msg(buf);
    return NULL;
}

/* ============================================================
 *  MAIN
 * ============================================================ */
int main(void) {
    srand((unsigned int)time(NULL));

    log_fp = fopen(LOG_FILE, "w");
    if (!log_fp) { perror("fopen bitacora"); return 1; }

    fprintf(log_fp,
            "=================================================================\n"
            "  BITACORA - Programa 2: Monitor (mutex + cond variable)\n"
            "  CC3064 Sistemas Operativos - Laboratorio #5\n"
            "=================================================================\n"
            "  Recursos disponibles : %d\n"
            "  Threads              : %d\n"
            "  Iteraciones/thread   : %d\n"
            "  Max recursos/consume : %d\n"
            "=================================================================\n\n",
            NUM_RESOURCES, NUM_THREADS, NUM_ITERATIONS, MAX_CONSUME);
    fflush(log_fp);

    printf("Iniciando programa\n");
    printf("Recursos: %d | Threads: %d | Iteraciones: %d | Max consume: %d\n\n",
           NUM_RESOURCES, NUM_THREADS, NUM_ITERATIONS, MAX_CONSUME);

    pthread_t threads[NUM_THREADS];
    log_msg("Creando threads");

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, thread_func, (void *)i);
    }

    /* main solo espera */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    log_msg("\nTodos los threads terminaron correctamente.");
    fprintf(log_fp,
            "\n=================================================================\n"
            "  FIN DE BITACORA\n"
            "=================================================================\n");

    pthread_mutex_destroy(&monitor_mutex);
    pthread_cond_destroy(&resource_cond);
    fclose(log_fp);
    return 0;
}
