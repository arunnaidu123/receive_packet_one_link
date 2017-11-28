receive_packet: receive_packet.cpp
	g++ -g -Wall -o receive_packet receive_packet.cpp -std=c++11 -lpthread

clean:
	rm -rf *.o
