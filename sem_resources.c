#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

/* ============================================================
 *  PARAMETROS AJUSTABLES
 * ============================================================ */
#define NUM_THREADS     10   /* Numero de threads a crear            */
#define NUM_ITERATIONS   5   /* Iteraciones que ejecuta cada thread  */
#define NUM_RESOURCES    3   /* Recursos digitales disponibles       */
#define MAX_SLEEP_MS   500   /* Maximo tiempo de uso del recurso(ms) */
#define MIN_SLEEP_MS    50   /* Minimo tiempo de uso del recurso(ms) */
#define LOG_FILE        "bitacora_semaforos.txt"

/* ============================================================
 *  VARIABLES GLOBALES
 * ============================================================ */
static int             available_resources = NUM_RESOURCES; /* solo display  */
static sem_t           resource_sem;                        /* semaforo POSIX */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE           *log_fp;

/* ------------------------------------------------------------
 * log_msg: escribe en stdout y en el archivo de bitacora de
 * forma atomica (protegido por log_mutex).
 * ------------------------------------------------------------ */
static void log_msg(const char *msg) {
    pthread_mutex_lock(&log_mutex);
    fprintf(log_fp, "%s\n", msg);
    fflush(log_fp);
    printf("%s\n", msg);
    pthread_mutex_unlock(&log_mutex);
}

/* ------------------------------------------------------------
 * thread_func: rutina ejecutada por cada thread.
 *
 * Por cada iteracion:
 *   1. sem_wait()  -> bloquea si no hay recursos libres.
 *   2. Consume el recurso (decrementa contador de display).
 *   3. Simula trabajo con usleep() aleatorio.
 *   4. Devuelve el recurso (incrementa contador).
 *   5. sem_post()  -> libera el semaforo para otro thread.
 * ------------------------------------------------------------ */
static void *thread_func(void *arg) {
    long tid = (long)arg;
    char buf[256];

    snprintf(buf, sizeof(buf), "Iniciando thread %ld", tid);
    log_msg(buf);

    for (int i = 0; i < NUM_ITERATIONS; i++) {

        /* -- Solicitar recurso -------------------------------- */
        snprintf(buf, sizeof(buf),
                 "Thread %ld | Iter %d - Semaforo abierto con exito.",
                 tid, i + 1);
        /* Avisar antes de esperar */
        {
            pthread_mutex_lock(&log_mutex);
            fprintf(log_fp, "Thread %ld | Iter %d - Esperando semaforo (disponibles: %d)...\n",
                    tid, i + 1, available_resources);
            printf("Thread %ld | Iter %d - Esperando semaforo (disponibles: %d)...\n",
                   tid, i + 1, available_resources);
            fflush(log_fp);
            pthread_mutex_unlock(&log_mutex);
        }

        sem_wait(&resource_sem); /* <-- bloquea si sem == 0 */

        /* Seccion critica de display */
        pthread_mutex_lock(&log_mutex);
        available_resources--;
        fprintf(log_fp,
                "Thread %ld | Iter %d - Semaforo abierto con exito.\n"
                "Thread %ld | Iter %d - (+) Recurso TOMADO.    Disponibles: %d/%d\n",
                tid, i + 1, tid, i + 1, available_resources, NUM_RESOURCES);
        printf("Thread %ld | Iter %d - Semaforo abierto con exito.\n"
               "Thread %ld | Iter %d - (+) Recurso TOMADO.    Disponibles: %d/%d\n",
               tid, i + 1, tid, i + 1, available_resources, NUM_RESOURCES);
        fflush(log_fp);
        pthread_mutex_unlock(&log_mutex);

        /* -- Simular trabajo ---------------------------------- */
        int sleep_ms = (rand() % (MAX_SLEEP_MS - MIN_SLEEP_MS + 1)) + MIN_SLEEP_MS;
        usleep((useconds_t)sleep_ms * 1000);

        /* -- Devolver recurso --------------------------------- */
        pthread_mutex_lock(&log_mutex);
        available_resources++;
        fprintf(log_fp,
                "Thread %ld | Iter %d - Buenos dias! Recurso usado.\n"
                "Thread %ld | Iter %d - (-) Recurso DEVUELTO. Disponibles: %d/%d\n",
                tid, i + 1, tid, i + 1, available_resources, NUM_RESOURCES);
        printf("Thread %ld | Iter %d - Buenos dias! Recurso usado.\n"
               "Thread %ld | Iter %d - (-) Recurso DEVUELTO. Disponibles: %d/%d\n",
               tid, i + 1, tid, i + 1, available_resources, NUM_RESOURCES);
        fflush(log_fp);
        pthread_mutex_unlock(&log_mutex);

        sem_post(&resource_sem); /* <-- libera el semaforo */
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
            "  BITACORA - Programa 1: Semaforos Pthreads\n"
            "  CC3064 Sistemas Operativos - Laboratorio #5\n"
            "=================================================================\n"
            "  Recursos disponibles : %d\n"
            "  Threads              : %d\n"
            "  Iteraciones/thread   : %d\n"
            "=================================================================\n\n",
            NUM_RESOURCES, NUM_THREADS, NUM_ITERATIONS);
    fflush(log_fp);

    printf("Iniciando programa\n");
    printf("Recursos: %d | Threads: %d | Iteraciones: %d\n\n",
           NUM_RESOURCES, NUM_THREADS, NUM_ITERATIONS);

    /* Inicializar semaforo con la cantidad de recursos disponibles */
    if (sem_init(&resource_sem, 0, NUM_RESOURCES) != 0) {
        perror("sem_init"); fclose(log_fp); return 1;
    }

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

    sem_destroy(&resource_sem);
    fclose(log_fp);
    return 0;
}
