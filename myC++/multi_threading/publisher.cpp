#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>


const char* SHM_NAME = "/my_shm";
const size_t SHM_SIZE = 1024;

struct SharedData {
    pthread_mutex_t mutex;
    char message[SHM_SIZE - sizeof(pthread_mutex_t)];
};


int main() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, SHM_SIZE);
    
    void* ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    SharedData* data = reinterpret_cast<SharedData*>(ptr);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&data->mutex, &attr);

    int count = 0;

    while(true) {


        pthread_mutex_lock(&data->mutex);

        std::string msg = "Hello " + std::to_string(count++);
        strncpy(data->message,  msg.c_str(), sizeof(data->message) - 1);
        data->message[sizeof(data->message) -1] = '\0';

        pthread_mutex_unlock(&data->mutex);

        std::cout << "[Publisher] Sent: " << msg << std::endl;
        sleep(1);
    }

    munmap(ptr, SHM_SIZE);
    close(fd);
    shm_unlink(SHM_NAME);
}

        
