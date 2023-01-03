#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>

#define PERIODO_A_SEC 90
#define PERIODO_A_NSEC 200000000
#define PRIORIDAD_A 24
#define ITER_A 40
#define INC_A 100

#define PERIODO_B_SEC 100
#define PERIODO_B_NSEC 300000000
#define PRIORIDAD_B 26
#define ITER_B 40
#define INC_B 1

struct Data
{
    pthread_mutex_t mutex;
    int cnt;
};

#define CHKN(syscall)                                     \
    do                                                    \
    {                                                     \
        int err = syscall;                                \
        if (err != 0)                                     \
        {                                                 \
            fprintf(stderr, "%s:%d:SysCall Error : %s\n", \
                    __FILE__, __LINE__, strerror(errno)); \
            exit(EXIT_FAILURE);                           \
        }                                                 \
    } while (0)
/*----------------------------------------------------------*/
#define CHKE(syscall)                                                                           \
    do                                                                                          \
    {                                                                                           \
        int err = syscall;                                                                      \
        if (err != 0)                                                                           \
        {                                                                                       \
            fprintf(stderr, " %s: %d: SysCall Error: %s\n", __FILE__, __LINE__, strerror(err)); \
            exit(EXIT_FAILURE);                                                                 \
        }                                                                                       \
    } while (0)
/*----------------------------------------------------------*/
void espera_activa(time_t seg)
{
    volatile time_t t = time(0) + seg;
    while (time(0) < t)
    {
    }
}
/*----------------------------------------------------------*/
const char *get_time(char *buf)
{
    time_t t = time(0);
    char *f = ctime_r(&t, buf);
    f[strlen(f) - 1] = '\0';
    return f;
}
/*----------------------------------------------------------*/
void addtime(struct timespec *tm, const struct timespec *val)
{
    tm->tv_sec += val->tv_sec;
    tm->tv_nsec += val->tv_nsec;

    if (tm->tv_nsec >= 1000000000L)
    {
        tm->tv_sec += (tm->tv_nsec / 1000000000L);
        tm->tv_nsec = (tm->tv_nsec % 1000000000L);
    }
}
/*----------------------------------------------------------*/
int clock_nanosleep_intr(clockid_t clk_id, int flags, const struct timespec *request, struct timespec *remain)
{
    int err;
    while ((err = clock_nanosleep(clk_id, flags, request, remain)) == EINTR)
    {
    }
    return err;
}
/*----------------------------------------------------------*/
// TAREAS//
/*----------------------------------------------------------*/
void *tarea_a(void *arg)
{
    const struct timespec periodo = {PERIODO_A_SEC, PERIODO_A_NSEC};
    struct timespec next;
    char buf[30];
    struct Data *data = arg;
    struct sched_param param;
    const char *pol;
    int i, policy;
    CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
    pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR"
                                                               : "--";

    CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
    while (1)
    {
        CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
        addtime(&next, &periodo);
        printf("Tarea A [%s]\n", get_time(buf));
    }

    for (i = 0; i < ITER_A; ++i)
    {
        CHKE(pthread_mutex_lock(&data->mutex));
        data->cnt += INC_A;
        printf("Tarea A [%s: %d]: %d\n", pol, param.sched_priority, data->cnt);
        CHKE(pthread_mutex_unlock(&data->mutex));
        espera_activa(1);
    }
}
/*----------------------------------------------------------*/
void *tarea_b(void *arg)
{
    const struct timespec periodo = {PERIODO_B_SEC, PERIODO_B_NSEC};
    struct timespec next;
    char buf[30];
    struct Data *data = arg;
    struct sched_param param;
    const char *pol;
    int i, policy;
    CHKE(pthread_getschedparam(pthread_self(), &policy, &param));
    pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR"
                                                               : "--";

    CHKN(clock_gettime(CLOCK_MONOTONIC, &next));
    while (1)
    {
        CHKE(clock_nanosleep_intr(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL));
        addtime(&next, &periodo);
        printf("Tarea B [%s]\n", get_time(buf));
    }

    for (i = 0; i < ITER_B; ++i)
    {
        CHKE(pthread_mutex_lock(&data->mutex));
        data->cnt += INC_B;
        printf("Tarea B [%s: %d]: %d\n", pol, param.sched_priority, data->cnt);
        CHKE(pthread_mutex_unlock(&data->mutex));
        espera_activa(1);
    }
}
/*----------------------------------------------------------*/
// PROGRAMA PRINCIPAL//
/*----------------------------------------------------------*/
void usage(const char *nm)
{
    fprintf(stderr, "usage: %s [-h] [-ff] [-rr] [-p1] [-p2] \n", nm);
    exit(EXIT_FAILURE);
}
/*----------------------------------------------------------*/
void get_args(int argc, const char *argv[],
              int *policy, int *prio1, int *prio2)
{
    int i;
    if (argc < 2)
    {
        usage(argv[0]);
    }
    else
    {
        for (i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "-h") == 0)
            {
                usage(argv[0]);
            }
            else if (strcmp(argv[i], "-ff") == 0)
            {
                *policy = SCHED_FIFO;
            }
            else if (strcmp(argv[i], "-rr") == 0)
            {
                *policy = SCHED_RR;
            }
            else if (strcmp(argv[i], "-p1") == 0)
            {
                *prio1 = PRIORIDAD_A;
                *prio2 = PRIORIDAD_B;
            }
            else if (strcmp(argv[i], "-p2") == 0)
            {
                *prio1 = PRIORIDAD_B;
                *prio2 = PRIORIDAD_A;
            }
            else
            {
                usage(argv[0]);
            }
        }
    }
}
/*----------------------------------------------------------*/
int main(int argc, const char *argv[])
{
    struct Data shared_data;
    pthread_attr_t attr;
    struct sched_param param;
    const char *pol;
    int pcy, policy = SCHED_FIFO;
    int prio0 = 1, prio1 = 1, prio2 = 1;
    pthread_t t1, t2;

    get_args(argc, argv, &policy, &prio1, &prio2);
    prio0 = (prio1 > prio2 ? prio1 : prio2) + 1;

    CHKN(mlockall(MCL_CURRENT | MCL_FUTURE));
    CHKE(pthread_getschedparam(pthread_self(), &pcy, &param));
    param.sched_priority = prio0;
    CHKE(pthread_setschedparam(pthread_self(), policy, &param));
    pol = (policy == SCHED_FIFO) ? "FF" : (policy == SCHED_RR) ? "RR"
                                                               : "--";
    shared_data.cnt = 0;
    CHKE(pthread_mutex_init(&shared_data.mutex, NULL));

    CHKE(pthread_attr_init(&attr));
    CHKE(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED));
    CHKE(pthread_attr_setschedpolicy(&attr, policy));
    param.sched_priority = prio1;
    CHKE(pthread_attr_setschedparam(&attr, &param));
    CHKE(pthread_create(&t1, &attr, tarea_a, &shared_data));
    param.sched_priority = prio2;
    CHKE(pthread_attr_setschedparam(&attr, &param));
    CHKE(pthread_create(&t2, &attr, tarea_b, &shared_data));
    CHKE(pthread_attr_destroy(&attr));

    printf("Tarea principal [%s: %d]\n", pol, prio0);
    CHKE(pthread_join(t1, NULL));
    CHKE(pthread_join(t2, NULL));
    CHKE(pthread_mutex_destroy(&shared_data.mutex));
    return 0;
}