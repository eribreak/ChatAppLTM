chay client 
LD_PRELOAD=/lib/x86_64-linux-gnu/libpthread.so.0 ./client
gcc -o chat_client client.c `pkg-config --cflags --libs gtk+-3.0` -pthread
