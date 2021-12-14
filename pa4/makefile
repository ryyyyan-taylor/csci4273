make: ./dfs.c ./dfc.c
	gcc -w dfs.c -pthread -o dfs
	gcc -w dfc.c -lcrypto -o dfc

clean:
	rm dfs
	rm dfc