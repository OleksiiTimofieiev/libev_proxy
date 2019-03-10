NAME = proxy

all: 
	g++ -std=c++11 proxy.cpp -o $(NAME) -lev -lpthread
clean:
	rm -f $(NAME)
	rm -f log.txt

re: clean all

.PHONY: all clean re
