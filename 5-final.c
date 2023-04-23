#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

typedef struct {
    int item_value;
    int total_value;
    sem_t sem_ivan;
    sem_t sem_petr;
    sem_t sem_alex;
} SharedData;

volatile sig_atomic_t interrupted = 0;

void signal_handler(int signal) {
    interrupted = 1;

    shm_unlink("/shared_memory");

    printf("Прерывание с клавиатуры\n");

    exit(0);
}

void ivan(sem_t *sem_ivan, sem_t *sem_petr, SharedData *data) {
    while (!interrupted) {
        sem_wait(sem_ivan);
        if (interrupted) break;

        data->item_value = rand() % 100;
        printf("Иван передал имущество стоимостью %d.\n", data->item_value);
        sleep(rand() % 4);
        sem_post(sem_petr);
    }
}

void petr(sem_t *sem_petr, sem_t *sem_alex, SharedData *data) {
    while (!interrupted) {
        sem_wait(sem_petr);
        if (interrupted) break;

        printf("Петр грузит имущество стоимостью %d.\n", data->item_value);
        sleep(rand() % 4);
        sem_post(sem_alex);
    }
}

void alexandr(sem_t *sem_alex, sem_t *sem_ivan, SharedData *data) {
    while (!interrupted) {
        sem_wait(sem_alex);
        if (interrupted) break;

        data->total_value += data->item_value;
        printf("Александр подсчитал стоимость: %d.\n", data->total_value);
        sleep(rand() % 4);
        sem_post(sem_ivan);
    }
}

int main() {
    // Инициализация разделяемой памяти
    int shared_memory_fd = shm_open("/shared_memory", O_CREAT | O_RDWR, 0644);
    ftruncate(shared_memory_fd, sizeof(SharedData));
    SharedData *data = (SharedData *) mmap(0, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd,
                                           0);

    // Инициализация семафоров
    sem_init(&data->sem_ivan, 1, 1);
    sem_init(&data->sem_petr, 1, 0);
    sem_init(&data->sem_alex, 1, 0);

    // Установка обработчика сигнала
    signal(SIGINT, signal_handler);

    // Создание дочерних процессов
    pid_t pid;

    for (int i = 0; i < 3; i++) {
        pid = fork();
        if (pid == 0) {
            if (i == 0) {
                ivan(&data->sem_ivan, &data->sem_petr, data);
            } else if (i == 1) {
                petr(&data->sem_petr, &data->sem_alex, data);
            } else {
                alexandr(&data->sem_alex, &data->sem_ivan, data);
            }

            exit(0);
        }
    }

    // Ожидание завершения дочерних процессов
    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }

    // Закрытие и удаление семафоров
    sem_destroy(&data->sem_ivan);
    sem_destroy(&data->sem_petr);
    sem_destroy(&data->sem_alex);

    // Закрытие и удаление разделяемой памяти
    munmap(data, sizeof(SharedData));
    shm_unlink("/shared_memory");

    return 0;
}
