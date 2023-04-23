#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

typedef struct {
    int item_value;
    int total_value;
} SharedData;

volatile sig_atomic_t interrupted = 0;

void signal_handler(int signal) {
    interrupted = 1;

    printf("Прерывание с клавиатуры\n");

    exit(0);
}

void P(int sem_id, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = -1;
    op.sem_flg = 0;
    semop(sem_id, &op, 1);
}

void V(int sem_id, int sem_num) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = 1;
    op.sem_flg = 0;
    semop(sem_id, &op, 1);
}

void ivan(int sem_id, int sem_ivan, int sem_petr, SharedData *data) {
    while (!interrupted) {
        P(sem_id, sem_ivan);
        if (interrupted) break;

        data->item_value = rand() % 100;
        printf("Иван передал имущество стоимостью %d.\n", data->item_value);
        sleep(rand() % 4);
        V(sem_id, sem_petr);
    }
}

void petr(int sem_id, int sem_petr, int sem_alex, SharedData *data) {
    while (!interrupted) {
        P(sem_id, sem_petr);
        if (interrupted) break;

        printf("Петр грузит имущество стоимостью %d.\n", data->item_value);
        sleep(rand() % 4);
        V(sem_id, sem_alex);
    }
}

void alexandr(int sem_id, int sem_alex, int sem_ivan, SharedData *data) {
    while (!interrupted) {
        P(sem_id, sem_alex);
        if (interrupted) break;

        data->total_value += data->item_value;
        printf("Александр подсчитал стоимость: %d.\n", data->total_value);
        sleep(rand() % 4);
        V(sem_id, sem_ivan);
    }
}

int main() {
    // Инициализация разделяемой памяти
    key_t shm_key = ftok("main", 1);
    int shared_memory_id = shmget(shm_key, sizeof(SharedData), IPC_CREAT | 0644);
    SharedData *data = (SharedData *) shmat(shared_memory_id, NULL, 0);

    // Инициализация семафоров
    key_t sem_key = ftok("main", 2);
    int sem_id = semget(sem_key, 3, IPC_CREAT | 0644);
    semctl(sem_id, 0, SETVAL, 1);
    semctl(sem_id, 1, SETVAL, 0);
    semctl(sem_id, 2, SETVAL, 0);

    // Установка обработчика сигнала
    signal(SIGINT, signal_handler);

    // Создание дочерних процессов
    pid_t pid;

    for (int i = 0; i < 3; i++) {
        pid = fork();
        if (pid == 0) {
            if (i == 0) {
                ivan(sem_id, 0, 1, data);
            } else if (i == 1) {
                petr(sem_id, 1, 2, data);
            } else {
                alexandr(sem_id, 2, 0, data);
            }
            shmdt(data);
            exit(0);
        }
    }

    // Ожидание завершения дочерних процессов
    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }

    // Закрытие и удаление разделяемой памяти
    shmdt(data);
    shmctl(shared_memory_id, IPC_RMID, NULL);

    // Удаление семафоров
    semctl(sem_id, 0, IPC_RMID, 0);

    return 0;
}

