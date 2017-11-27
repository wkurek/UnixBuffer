#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>


#define N 8 //buffer size
#define MAX_PRODUCTION_SIZE 3
#define ALPHABET_LENGTH 26
#define TEST_STEPS 12


typedef struct {
    char label;
    unsigned int lastReadBy;//indicates which consumer read it
} Element;

typedef struct {
    Element buffer[N]; //buffer of N elements
    sem_t empty, full, mutex; //unnamed semaphores
    sem_t empty_size;
    int head, tail; //relative pointers
    int count; //number of filled elements in buffer
} Buffer;

void producer1Task(Buffer *buffer) {
    for(;;) {
      int size, i;

      srand(17);

      sem_wait(&(buffer->mutex));
      size = rand() % (MAX_PRODUCTION_SIZE);
      size += 1;//Avoid 0 size

      printf("Producer1 wants to produce %d elements - free space = %d\n", size, (N - buffer->count));

      while((N - buffer->count) < size) {
        sem_post(&(buffer->mutex));
        sem_wait(&(buffer->empty_size));
        printf("Producer1 wants to produce %d elements - free space = %d\n", size, (N - buffer->count));
        sem_wait(&(buffer->mutex));
      }

      for(i = 0; i < size; ++i) {
        sem_wait(&(buffer->empty));
        buffer->buffer[buffer->tail].label = 'a' + (rand() % ALPHABET_LENGTH);
        buffer->buffer[buffer->tail].lastReadBy = 0;
        ++buffer->count;
          printf("Producer1:\t%c\t[%d]\n", buffer->buffer[buffer->tail].label,
            buffer->count);
        buffer->tail = (buffer->tail + 1) % N;
        sem_post(&(buffer->full));
      }
      sem_post(&(buffer->mutex));
      sleep(2);
    }
}

void producer2Task(Buffer *buffer) {
  for(;;) {
      int size, i;

      srand(34);

      sem_wait(&(buffer->mutex));
      size = rand() % (MAX_PRODUCTION_SIZE);
      size += 1;//Avoid 0 size

      printf("Producer2 wants to produce %d elements - free space = %d\n", size, (N - buffer->count));

      while((N - buffer->count) < size) {
        sem_post(&(buffer->mutex));
        sem_wait(&(buffer->empty_size));
        printf("Producer2 wants to produce %d elements - free space = %d\n", size, (N - buffer->count));
        sem_wait(&(buffer->mutex));
      }

      for(i = 0; i < size; ++i) {
        sem_wait(&(buffer->empty));
        buffer->buffer[buffer->tail].label = 'a' + (rand() % ALPHABET_LENGTH);
        buffer->buffer[buffer->tail].lastReadBy = 0;
        ++buffer->count;
          printf("Producer2:\t%c\t[%d]\n", buffer->buffer[buffer->tail].label,
            buffer->count);
        buffer->tail = (buffer->tail + 1) % N;
        sem_post(&(buffer->full));
      }
      sem_post(&(buffer->mutex));
      sleep(1);
    }
}

void consumer1Task(Buffer *buffer) {
    unsigned int id;
    id = 1;

    for(;;) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));

        if(buffer->buffer[buffer->head].lastReadBy == (id-1) ||
              buffer->buffer[buffer->head].lastReadBy == id) { //Read label

            printf("Consumer%d:\tread: %c\t[%d]\n", id, buffer->buffer[buffer->head].label,
              buffer->count);
            buffer->buffer[buffer->head].lastReadBy = id;
        }

        sem_post(&(buffer->mutex));
        sem_post(&(buffer->full));
        sleep(2);
    }
}

void consumer2Task(Buffer *buffer) {
    unsigned int id;
    id = 2;

    for(;;) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));

        if(buffer->buffer[buffer->head].lastReadBy == (id-1) ||
              buffer->buffer[buffer->head].lastReadBy == id-1) { //Read label

            printf("Consumer%d:\tread: %c\t[%d]\n", id, buffer->buffer[buffer->head].label,
              buffer->count);
            buffer->buffer[buffer->head].lastReadBy = id;
        }

        sem_post(&(buffer->mutex));
        sem_post(&(buffer->full));
        sleep(3);
    }
}

void consumer3Task(Buffer *buffer) {
    unsigned int id;
    id = 3;

    for(;;) {
        sem_wait(&(buffer->full));
        sem_wait(&(buffer->mutex));

        if(buffer->buffer[buffer->head].lastReadBy == (id-1) && buffer->count > 4) { //Read label and delete element from buffer
            --buffer->count;
            printf("Consumer%d:\tread and remove: %c\t[%d]\n", id,
              buffer->buffer[buffer->head].label, buffer->count);
            buffer->head = (buffer->head + 1) % N;

            sem_post(&(buffer->mutex));
            sem_post(&(buffer->empty));
            sem_post(&(buffer->empty_size));
        } else {
            sem_post(&(buffer->mutex));
            sem_post(&(buffer->full));
        }
        sleep(2);
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
    if((sem_init(&(buffer->empty_size), 1, 0)) == -1){
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
