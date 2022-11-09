// Dominik Matijaca 0036524568
#include <err.h>
#include <errno.h>
#include <signal.h>
void sigset(int sig, void (*disp)());
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>

#define MAX_TEXT 5
struct {
    long type;
    char text[MAX_TEXT];
} msg;

enum MSG_TYPE {
    MSG_OFFER = 1,
    MSG_TAKING = 2,
};

int msq = -1;
void send() {
    int success = msgsnd(msq, &msg, sizeof(msg), 0);
    if (success == -1) exit(0);
}
void recv(long type) {
    int success = msgrcv(msq, &msg, sizeof(msg), type, 0);
    if (success == -1) exit(0);
}

const char* const sastojci[] = {"papir", "duhan", "šibice"};
#define N ((int)(sizeof(sastojci) / sizeof(sastojci[0])))
const char* const trazim[N] = {"12", "02", "01"};

pid_t pid[N];

#define MAX_NAME 15
char whoami[MAX_NAME] = "TRGOVAC";

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

void dispose_child() {
    exit(0);
}

void dispose() {
    if (msq > -1) msgctl(msq, IPC_RMID, NULL);

    for (int i = 0; i < N; i++)
        kill(pid[i], SIGTERM);
    
    exit(0);
}

int child(int id) {
    sigset(SIGINT, dispose_child);
    sigset(SIGTERM, dispose_child);

    sprintf(whoami, "PUŠAČ %d", id + 1);
    
    print("Pozdrav!");

    char x = 0;
    while (1) {
        recv(MSG_OFFER);
        if (x == msg.text[2]) {
            sleep(1);
            send();
            continue;
        }
        x = msg.text[2];

        if (memcmp(msg.text, trazim[id], 2)) {
            print("Nije za mene");
            send();

        } else {
            print("Uzimam i pušim, hvala!");
            msg.type = MSG_TAKING;
            send();
            sleep(3);
            print("Popušio.");
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    start = get_time();

    msq = msgget(ftok(argv[0], 0), 0666 | IPC_CREAT);
    if (msq == -1) err(1, "msgget");

    sigset(SIGINT, dispose);
    sigset(SIGTERM, dispose);

    for (int i = 0; i < N; i++) {
        pid[i] = fork();
        if (pid[i] < 0) err(1, "fork");
        if (pid[i] == 0) exit(child(i));
    }
    
    // trgovac
    srand(time(NULL));
    print("Pozdrav!");

    for (unsigned char x = 1;; x++) {
        if (x == 0) x++;

        sleep(1);

        int i = rand() % N;
        int j = rand() % N;
        if (i == j) continue;
        if (i > j) { int t = i; i = j; j = t; }

        print("Nudim %s i %s", sastojci[i], sastojci[j]);

        msg.type = MSG_OFFER;
        msg.text[0] = 0x30 + i;
        msg.text[1] = 0x30 + j;
        msg.text[2] = x;
        msg.text[3] = '\0';
        send();

        recv(MSG_TAKING);
        print("Uspješno prodano");
    }

    dispose();
    return 0;
}