
#include <pthread.h>
#include <stdio.h>

void* threadA(void* arg) {
    while (1) {
        printf("Thread A is running\n");
    }
    return NULL;
}

void* threadB(void* arg) {
    while (1) {
        printf("Thread B is running\n");
    }
    return NULL;
}

int main() {
    pthread_t A, B;

    pthread_create(&A, NULL, threadA, NULL);
    pthread_create(&B, NULL, threadB, NULL);

    pthread_join(A, NULL);
    pthread_join(B, NULL);

    return 0;
}
