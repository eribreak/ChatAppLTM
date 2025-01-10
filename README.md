# Create database
mysql -u root -p (Enter password)
create_database.sql

Neu co dat password thi phai vao ham main() trong main.c de thay doi connect_db

# Run server
cd server
make
./server

# Run client
cd client
make
LD_PRELOAD=/lib/x86_64-linux-gnu/libpthread.so.0 ./client
