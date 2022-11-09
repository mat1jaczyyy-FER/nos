// Dominik Matijaca 0036524568
#include <err.h>
#include <pthread.h>
#include <signal.h>
void sigset(int sig, void (*disp)());
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

//#define LOG_PIPES
//#define LOG_QUEUE
//#define LOG_RELEASE

#define CS_DURATION 200000 // microseconds

#define MAX_NAME 7
char whoami[MAX_NAME] = "MAIN";

double start;

double get_time() {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return now.tv_sec + now.tv_nsec*1e-9;
}

void print(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("% 9.5lf [%s] ", get_time() - start, whoami);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

int N;

typedef struct {
    int r, w;
} pipe_t;
pipe_t* pfd = NULL;

typedef struct {
    char sender;
    char type;
} msg_t;

enum CS_MSG_TYPE {
    CS_REQUEST,
    CS_REPLY,
    CS_RELEASE,
    DONE,
    WAIT_FOR_TOP,
    ON_TOP,
    ALL_DONE
};

const char* const MSGS[] = {
    "CS_REQUEST",
    "CS_REPLY",
    "CS_RELEASE",
    "DONE",
    "WAIT_FOR_TOP",
    "ON_TOP",
    "ALL_DONE"
};

void pipe_log(msg_t* msg) {
    #ifdef LOG_PIPES
        print("<-- [P%02d] !%s", msg->sender, MSGS[msg->type]);
    #endif
}

void pipe_write(int i, msg_t* msg) {
    #ifdef LOG_PIPES
        print("--> [P%02d]  %s", i, MSGS[msg->type]);
    #endif
    write(pfd[i].w, msg, sizeof(*msg));
}

void pipe_broadcast(msg_t* msg) {
    for (int i = 0; i < N; ++i) {
        pipe_write(i, msg);
    }
}

typedef struct {
    int pid;
    int ts;
    int cnt;
} row;
row* db;
int shm;

void dispose() {
    shmdt(db);
    shmctl(shm, IPC_RMID, NULL);
    
    if (pfd) free(pfd);

    exit(0);
}

typedef struct {
    int ts;
    int id;
} waiting;

void queue_log(int id, waiting* q, int s) {
    printf("\t\t\t\t\t\tq%02d: ", id);
    if (s == 0) {
        printf("empty");
    } else {
        for (int i = 0; i < s; i++) {
            printf("%d ", q[i].id);
        }
    }
    printf("\n");
}

pipe_t tp;

void* qthread(void* arg) {
    int id = *(int*)arg;
    waiting q[N];
    int s = 0;
    int gts = 1;
    int notify_on_top = 0;
    int done = 0;

    msg_t msg;

    while (1) {
        int n = read(pfd[id].r, &msg, sizeof(msg));
        if (n == 0) break;
        if (n == -1) err(1, "read");
        
        if (msg.type <= DONE) {
            pipe_log(&msg);
        }

        if (msg.type == CS_REQUEST) {
            int sender = msg.sender;
            q[s++] = (waiting){gts++, sender};
            
            #ifdef LOG_QUEUE
                queue_log(id, q, s);
            #endif

            msg.sender = id;
            msg.type = CS_REPLY;
            pipe_write(sender, &msg);
        
        } else if (msg.type == CS_REPLY) {
            write(tp.w, &msg, sizeof(msg));

        } else if (msg.type == WAIT_FOR_TOP) {
            if (msg.sender != id) continue;

            if (q[0].id == id) {
                msg.type = ON_TOP;
                write(tp.w, &msg, sizeof(msg));
                write(tp.w, &q[0].ts, sizeof(q[0].ts));

            } else {
                notify_on_top = 1;
            }

        } else if (msg.type == CS_RELEASE) {
            memcpy(q, q + 1, sizeof(q[0]) * --s);
            
            #if defined(LOG_QUEUE) || defined(LOG_RELEASE)
                queue_log(id, q, s);
            #endif

            if (notify_on_top && q[0].id == id) {
                msg.sender = id;
                msg.type = ON_TOP;
                write(tp.w, &msg, sizeof(msg));
                write(tp.w, &q[0].ts, sizeof(q[0].ts));
                notify_on_top = 0;
            }
        
        } else if (msg.type == DONE) {
            if (++done == N) {
                msg.sender = id;
                msg.type = ALL_DONE;
                write(tp.w, &msg, sizeof(msg));
                break;
            }
        }
    }

    return NULL;
}

int child(int id) {
    sprintf(whoami, "P%02d", id);
    srand(getpid());

    if (pipe((int*)(&tp)) == -1) err(1, "threadpipe %d", id);

    pthread_t qt;
    if (pthread_create(&qt, NULL, qthread, &id) ) err(1, "pthread_create %d", id);

    msg_t msg;

    for (int _ = 0; _ < 5; _++) {
        usleep(100000 + rand() % 1900000);

        msg.sender = id;
        msg.type = CS_REQUEST;
        pipe_broadcast(&msg);

        for (int i = 0; i < N; i += msg.type == CS_REPLY) {
            int n = read(tp.r, &msg, sizeof(msg));
            if (n == 0) exit(0);
            if (n == -1) err(1, "read");
        }

        msg.sender = id;
        msg.type = WAIT_FOR_TOP;
        write(pfd[id].w, &msg, sizeof(msg));

        while (msg.type != ON_TOP) {
            int n = read(tp.r, &msg, sizeof(msg));
            if (n == 0) exit(0);
            if (n == -1) err(1, "read");
        }
        
        int ts;
        int n = read(tp.r, &ts, sizeof(ts));
        if (n == 0) exit(0);
        if (n == -1) err(1, "read");

        db[id].cnt++;
        db[id].ts = ts;

        print("Critical section {");
        printf("\t\t\t\t\t\t-id-\t-pid-\t-ts-\t-cnt-\n");
        for (int i = 0; i < N; i++) {
            usleep(CS_DURATION / N);
            printf("\t\t\t\t\t\t%4d\t%5d\t%4d\t%5d\n", i, db[i].pid, db[i].ts, db[i].cnt);
        }
        print("}");

        msg.sender = id;
        msg.type = CS_RELEASE;
        pipe_broadcast(&msg);
    }

    msg.type = DONE;
    pipe_broadcast(&msg);

    while (msg.type != ALL_DONE) {
        int n = read(tp.r, &msg, sizeof(msg));
        if (n == 0) exit(0);
        if (n == -1) err(1, "read");
    }

    return 0;
}

int main(int argc, char* argv[]) {
    start = get_time();

    N = atoi(argv[1]);
    
    shm = shmget(IPC_PRIVATE, sizeof(*db) * N, 0600);
    if (shm == -1) err(1, "mem_create");

    db = (row*)shmat(shm, NULL, 0);
    
    sigset(SIGINT, dispose);

    pfd = malloc(sizeof(*pfd) * N);
    for (int i = 0; i < N; i++) {
        if (pipe((int*)(pfd + i)) == -1) err(1, "pipe %d", i);
    }

    print("Creating %d processes", N);

    for (int i = 0; i < N; i++) {
        db[i].ts = 0;
        db[i].cnt = 0;

        int pid = fork();
        if (pid < 0) err(1, "fork");
        if (pid == 0) exit(child(i));

        db[i].pid = pid;
    }

    print("Waiting...");

    for (int i = 0; i < N; i++)
        wait(NULL);

    dispose();
    return 0;
}