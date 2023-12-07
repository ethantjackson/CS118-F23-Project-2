CC=gcc
CFLAGS=-Wall -Wextra -Werror

all: clean build

default: build

build: server.cpp client.cpp
	g++ -Wall -Wextra -o server server.cpp CongestionController.cpp
	g++ -Wall -Wextra -o client client.cpp CongestionController.cpp

clean:
	rm -f server client output.txt project2.zip

zip: 
	zip project2.zip server.cpp client.cpp utils.h Makefile README
