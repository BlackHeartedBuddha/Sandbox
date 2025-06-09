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
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    void* ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    SharedData* data = reinterpret_cast<SharedData*>(ptr);

    std::string last_msg;


    while(true) {

        pthread_mutex_lock(&data->mutex);

        std::string msg(data->message);
        if(msg != last_msg) {
            std::cout << "[Subscriber] Received: " << msg << std::endl;
            last_msg = msg;
        }

        pthread_mutex_unlock(&data->mutex);
        usleep(100000);
    }

    munmap(ptr, SHM_SIZE);
    close(fd);

}


