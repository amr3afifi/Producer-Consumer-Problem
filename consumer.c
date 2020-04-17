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
#include <signal.h>
#include <semaphore.h>

int N=0;
int*globalcount;
int consindex=0;

union Semun
{
    int val;               		/* value for SETVAL */
    struct semid_ds *buf;  	/* buffer for IPC_STAT & IPC_SET */
    unsigned short *array;          	/* array for GETALL & SETALL */
    struct seminfo *__buf;  	/* buffer for IPC_INFO */
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

    if(semop(sem, &p_op, 1) == -1)
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

    if(semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

void consumer(char*array,int mutex)
{
    int rec_val,send_val;
    key_t si=ftok("producer.c",1);key_t empty=msgget(si,IPC_CREAT|0644);
    key_t ri=ftok("consumer.c",3);key_t full=msgget(ri,IPC_CREAT|0644);
    struct msgbuff message;
    while(1)
    {
        sleep(1);
        int iterations=0; message.mtype=1;
        down(mutex);
        if((*globalcount)!=0)
        {
            printf("\n GLOBAL %d\n",(*globalcount));
            for(int i=0; i<N; i++)
                printf("position %d = %c --\n",i, array[i]);

            printf("\nitem %c is consumed from %d\n",array[consindex],consindex);
            array[consindex]='~';
            consindex++;
            if(consindex==N)
            consindex=0;
            msgrcv(full, &message, sizeof(message.mtext), 0,IPC_NOWAIT);
            while(array[consindex]=='~' && iterations<N)
            { 
                if(consindex==N-1)
                consindex=0;
                else
                consindex++;

                iterations++;
            }
            if((*globalcount)==N)
            send_val = msgsnd(empty, &message,sizeof(message.mtext),IPC_NOWAIT);
            (*globalcount)--;
            up(mutex); 
        }
        else
        {
            up(mutex);
            printf("\nBuffer is empty waiting for producer\n");
            message.mtype=0;
            rec_val = msgrcv(full, &message, sizeof(message.mtext), 0,!IPC_NOWAIT);
        }
    }
}

int main()
{
    
    int rec_val;

    //receive size of shared array
    key_t pkey=ftok("producer.c",10); 
    key_t ckey=ftok("consumer.c",11);
    key_t pid=msgget(ckey,IPC_CREAT|0644);
    rec_val = msgrcv(pid, &N, sizeof(N), 0, !IPC_NOWAIT);
    msgctl(pid, IPC_RMID, (struct msqid_ds *) 0);
    key_t cid = shmget(pkey, N*sizeof(char), 0644|IPC_CREAT); //get buffer id
    printf("Buffer size = %d\n",N);
    //initialize shared array
    key_t arraykey=12345;
    int arrayid=shmget(arraykey,N*sizeof(char),IPC_CREAT | 0644);
    char*array=(char*)shmat(arrayid,0,0);

    //shared count of elements inside
    key_t countkey=50;
    int countid=shmget(countkey,sizeof(int),IPC_CREAT | 0644);
    globalcount=(int*)shmat(countid,0,0);

    //shared mutex
     key_t mutexkey=60;
    union Semun semun;
    int mutex = semget(mutexkey, 1, 0666|IPC_CREAT);
    if(mutex == -1)
    {perror("Error in create sem");exit(-1);}

    consumer(array,mutex);
    shmdt((void *) array);return 0;
}