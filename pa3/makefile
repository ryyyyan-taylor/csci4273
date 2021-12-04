#  __  __       _         __ _ _
# |  \/  | __ _| | _____ / _(_) | ___
# | |\/| |/ _` | |/ / _ \ |_| | |/ _ \
# | |  | | (_| |   <  __/  _| | |  __/
# |_|  |_|\__,_|_|\_\___|_| |_|_|\___|
#

make: ./webproxy.c
	gcc webproxy.c -pthread -lcrypto -o webproxy
clean:
	rm webproxy
	rm -rf cache
