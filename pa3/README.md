# PA3

Ryan Taylor  
CSCI 4273 : Network Systems  
Fall 2021  

## Building Server Binary
- From in the root directory, run ```make``` to build the ```proxy``` binary
- If needed, run ```make clean``` to remove the created binary

## Running Proxy Server
- command usage is ```./proxy <port> <timeout>```
- if run without timeout, timeout for each cache entry will be infinite


## Possible Tests
- enter telnet on localhost then run ```GET http://netsys.cs.colorado.edu HTTP/1.0```
	- this should return the entire HTML index file for the sample HTTP page
	- the server will return all the necessary information it retrieves
- set your web browser (methods vary) to use localhost as its proxy server and navigate to ```http://netsys.cs.colorado.edu```
	- the page should load in your browser with all links functioning
	- the server will return all necessary information it retrieves.