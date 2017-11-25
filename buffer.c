#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define N 4 //buffer size
#define MAX_LABEL_LENGTH 3
#define TEST_STEPS 12


typedef struct {
    char label[MAX_LABEL_LENGTH];
    unsigned int lastReadBy;//indicates which consumer read it
} Element;

typedef struct {
    Element buffer[N]; //buffer of N elements
    sem_t empty, full, mutex; //unnamed semaphores
    int head, tail; //relative pointers
    int count; //test purpose
} Buffer;

void producer1Task(Buffer *buffer) {
    for(;;) {
        sem_wait(&(buffer->empty));
        sem_wait(&(buffer->mutex));

            sprintf(buffer->buffer[buffer->tail].label,"%d",rand() % 10);
            buffer->buffer[buffer->tail].lastReadBy = 0;
            ++buffer->count;
            printf("PRODUCE: [%d] %s\n",buffer->count, buffer->buffer[buffer->tail].label);
            buffer->tail = (buffer->tail + 1) % N;

        sem_post(&(buffer->mutex));
        sem_post(&(buffer->full));
    }
}

void producer2Task(Buffer *buffer) {

}

void consumer1Task(Buffer *buffer) {
    unsigned int id;
    id = 1;

    for(;;) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));

        if(buffer->buffer[buffer->head].lastReadBy == (id-1)) { //Read label and delete element from buffer
            printf("CONSUMER%d: [%d] %s\n", id, buffer->count, buffer->buffer[buffer->head].label);
            buffer->buffer[buffer->head].lastReadBy = id;
        }

        sem_post(&(buffer->mutex));
        sem_post(&(buffer->full));
        sleep(1);
    }
}

void consumer2Task(Buffer *buffer) {
    unsigned int id;
    id = 2;

    for(;;) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));

        if(buffer->buffer[buffer->head].lastReadBy == (id-1)) { //Read label and delete element from buffer
            printf("CONSUMER%d: [%d] %s\n", id, buffer->count, buffer->buffer[buffer->head].label);
            buffer->buffer[buffer->head].lastReadBy = id;
        }

        sem_post(&(buffer->mutex));
        sem_post(&(buffer->full));
        sleep(1);
    }
}

void consumer3Task(Buffer *buffer) {
    unsigned int id;
    id = 3;

    for(;;) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));

        if(buffer->buffer[buffer->head].lastReadBy == (id-1)) { //Read label and delete element from buffer
            printf("CONSUMER%d: [%d] %s DELETED\n", id, buffer->count, buffer->buffer[buffer->head].label);
            buffer->head = (buffer->head + 1) % N;
            --buffer->count;

            sem_post(&(buffer->mutex));
            sem_post(&(buffer->empty));
        } else {
            sem_post(&(buffer->mutex));
            sem_post(&(buffer->full));
        }
        sleep(1);
    }
}



int main(int argc, char** argv)
{
    int fd, result;
    char *bufferName;
    Buffer *buffer;

    strcpy(bufferName, "buffer");
    shm_unlink(bufferName);

    if((fd = shm_open(bufferName, O_RDWR|O_CREAT, 0774)) == -1) {
        printf("Error: %d\n", errno);
        exit(-1);
    }

    if((ftruncate(fd, sizeof(Buffer))) < 0) {
        printf("Error: %d\n", errno);
        exit(-1);
    }

    buffer = (Buffer*) mmap(NULL, sizeof(Buffer), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(buffer == NULL) exit(-1);
    close(fd);

    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;

    if((sem_init(&(buffer->empty), 1, N)) == -1){
        printf("Error while initializing semaphore: %d\n", errno);
        exit(-1);
    }
    if((sem_init(&(buffer->full), 1, 0)) == -1){
        printf("Error while initializing semaphore: %d\n", errno);
        exit(-1);
    }
    if((sem_init(&(buffer->mutex), 1, 1)) == -1){
        printf("Error while initializing semaphore: %d\n", errno);
        exit(-1);
    }


    //Create 2 producers and 3 consumers
    result = fork(); //child process -> give task
    if(result == 0) { //Producer1
        producer1Task(buffer);

    } else {
        result = fork();
        if(result == 0) { //Producer2
                producer2Task(buffer);

        } else {
            result = fork();
            if(result == 0) { //Consumer1
                    consumer1Task(buffer);

            } else {
                result = fork();
                if(result == 0) { //Consumer2
                        consumer2Task(buffer);

                } else { //Consumer3
                    consumer3Task(buffer);

                }
            }

        }

    }



}
