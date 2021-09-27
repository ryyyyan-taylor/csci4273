Ryan Taylor  
CSCI 4273: Network Systems  
Programming Assignment 1  

# Compiling / Running
```make``` will compile server and client source code into server.o and client.o binaries in their respective directories

```make clean``` will delete both binaries  
<br>

Once compiled, run the server using  
```./server/server.o <PORT NUMBER>```  

and the client using  
```./client/client.o <SERVER IP> <PORT NUMBER>```

# Available Commands
On the client side, the commands available to the user are:
- ```get``` : copy file from server to client
- ```put``` : copy file from client to server
- ```delete``` : remove file from server
- ```ls``` : list all files in server directory