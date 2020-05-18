#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netinet/ip.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <fcntl.h>
#include <errno.h>


#include <iostream>
#include <tins/tins.h>
#include <thread>
#include <atomic>
#include<unistd.h>
using namespace Tins;
using namespace std;

#define MIN_PORT 30000
#define MAX_PORT 60000

int dst_port = 0;

atomic<unsigned long long> total(0);
atomic<int> warm_up(1);
atomic<int> run(1);

bool SetSocketBlockingEnabled(int fd)
{
   int flags = fcntl(fd,F_GETFL, 0);
   if (flags == -1) return false;
   flags |= O_NONBLOCK;

   return (fcntl(fd,F_SETFL, flags) == 0) ? true : false;
}

enum tcp_state {
	TCP_STATE_SYN = 0,
	TCP_STATE_ESTABLISHED,
	TCP_STATE_RST
};

struct tcp_state_info {
	atomic<int> state;
	unsigned long long timestamp;
	int port;
}; 

struct tcp_state_info tcs[MAX_PORT];

void recv_function() {
	unsigned char buffer[1024];
	PacketSender sender;
	int sock = socket (PF_INET,SOCK_RAW, IPPROTO_TCP);
	if(sock == -1)
	{
		perror("Failed to create socket");
		exit(1);
	}
	SetSocketBlockingEnabled(sock);
	while(warm_up);
	while(run) {
		int pkt_size = recv(sock ,buffer, 256, 0);
		if (pkt_size <=0 || pkt_size == -EAGAIN) {
			continue;
		}
		IP packet(buffer,pkt_size);
		TCP * t= packet.find_pdu<TCP>();
		if (t->get_flag(TCP::SYN) && t->dport() == dst_port) {
			int sport = t->sport();
			t->sport(t->dport());
			t->dport(sport);
			//t->set_flag();
			t->flags(TCP::SYN | TCP::ACK);
			//IP pkt = IP("127.0.0.1") / TCP(t->sport(),t->dport());
			//			pkt.rfind_pdu<TCP>().set_flag(TCP::SYN | TCP::ACK,1);
						//pkt.rfind_pdu<TCP>().set_flag(TCP::ACK,1);
			sender.send(packet);
				
		}
	}
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		cout<< "Help: " << argv[0] << " port_to_listen" << endl; 
		cout<< "Example: " << argv[0] << " 80 " << endl; 
		return 0;	
	}
	
	dst_port = stoi(argv[2]);

	for (int i = MIN_PORT; i < MAX_PORT; i++) {
		tcs[i].port = i;
	}
	std::thread recv_thread(recv_function);

	sleep(1);
	// wait for warm up
	warm_up.store(0);
	

        //sleep(5);

	// wait for run
	//run.store(0);

	recv_thread.join();
	
	cout << "stats" << total << endl;

	return 0;
}
