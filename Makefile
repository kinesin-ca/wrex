all: wrex

wrex: wrex.cpp
	g++ -O2 -Wall -o wrex wrex.cpp -lpthread
