make: client.c server.c udp.c mfs.c
	gcc -o client client.c udp.c mfs.c
	gcc -o server server.c udp.c
