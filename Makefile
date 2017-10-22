compile: server.o client.o

server.o: server.cpp
	g++ -o server server.cpp -lpthread
client.o: client.cpp
	g++ -o client client.cpp -lpthread
run_server: server.o
	/home/intekhab/UMBC/AOS/Project1/server 9011
run_client: client.o
	/home/intekhab/UMBC/AOS/Project1/client localhost 9011
clean:
	rm -rf server client
