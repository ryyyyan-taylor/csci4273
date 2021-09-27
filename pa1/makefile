#  __  __       _         __ _ _
# |  \/  | __ _| | _____ / _(_) | ___
# | |\/| |/ _` | |/ / _ \ |_| | |/ _ \
# | |  | | (_| |   <  __/  _| | |  __/
# |_|  |_|\__,_|_|\_\___|_| |_|_|\___|

make: server/uftpServer.c client/uftpClient.c
	gcc -o server/server.o server/uftpServer.c
	gcc -o client/client.o client/uftpClient.c

clean: server/server.o client/client.o
	rm server/server.o
	rm client/client.o