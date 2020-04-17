#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <ctype.h>
#include <sys/sem.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

int N = 0;
int *globalcount;
int prodindex = 0;

union Semun {
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    unsigned short *array; /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};

struct msgbuff
{
    long mtype;
    char mtext[100];
};

void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

void producer(char *array, int mutex)
{
    int send_val, rec_val;
    char c = 'a';
    key_t si = ftok("producer.c", 1);
    key_t empty = msgget(si, IPC_CREAT | 0644);
    key_t ri = ftok("consumer.c", 3);
    key_t full = msgget(ri, IPC_CREAT | 0644);
    struct msgbuff message;
    message.mtype = 1;
    //char c;
    for (int i = 0; i < N; i++)
        array[i] = '~';
    up(mutex);
    while (1)
    {
        sleep(1);
        down(mutex);
        if ((*globalcount) != N)
        {
            up(mutex);
            //printf("Enter 1 character to produce:");
            //scanf("%s", &c);
            c++;
            int iterations = 0;
            message.mtype = 1;
            down(mutex);
            array[prodindex] = c;
            printf("\nitem %c is produced at %d\n", array[prodindex], prodindex);
            prodindex++;
            if (prodindex == N)
                prodindex = 0;

            while (iterations < N && array[prodindex] != '~')
            {
                if (prodindex == N - 1)
                    prodindex = 0;
                else
                    prodindex++;

                iterations++;
            }
            if ((*globalcount) == 0)
                send_val = msgsnd(full, &message, sizeof(message.mtext), IPC_NOWAIT);
            (*globalcount)++;
            for (int i = 0; i < N; i++)
                printf("position %d = %c --\n", i, array[i]);

            up(mutex);
        }
        else
        {
            up(mutex);
            printf("\nBuffer is full waiting for consumer\n");
            message.mtype = 0;
            rec_val = msgrcv(empty, &message, sizeof(message.mtext), 0, !IPC_NOWAIT);
        }
    }
}

int main()
{
    int send_val;
    //Send size of shared array
    printf("Please enter buffer size(number of elements):");
    scanf("%d", &N);
    key_t pkey = ftok("producer.c", 10);                          //get creation key
    key_t cid = shmget(pkey, N * sizeof(char), 0644 | IPC_CREAT); //create and get buffer id
    key_t ckey = ftok("consumer.c", 11);
    key_t pid = msgget(ckey, IPC_CREAT | 0644);
    send_val = msgsnd(pid, &N, sizeof(N), !IPC_NOWAIT);

    //initialize shared array
    key_t arraykey = 12345;
    int arrayid = shmget(arraykey, N * sizeof(char), IPC_CREAT | 0644);
    char *array = (char *)shmat(arrayid, 0, 0);

    //shared count of elements inside
    key_t countkey = 50;
    int countid = shmget(countkey, sizeof(int), IPC_CREAT | 0644);
    globalcount = (int *)shmat(countid, 0, 0);
    (*globalcount) = 0;

    //shared mutex
    key_t mutexkey = 60;
    union Semun semun;

    int mutex = semget(mutexkey, 1, 0666 | IPC_CREAT);
    if (mutex == -1)
    {
        perror("Error in create sem");
        exit(-1);
    }

    semun.val = 0; //initial value
    if (semctl(mutex, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }

    producer(array, mutex);
    shmdt((void *)array);
    return 0;
}