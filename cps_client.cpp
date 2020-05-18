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

atomic<unsigned long long> total(0);
atomic<int> warm_up(1);
atomic<int> run(1);

char dst_ip[100];
int dst_port = 0;

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

bool callback(const PDU &pdu) {
    // Find the IP layer
    const IP &ip = pdu.rfind_pdu<IP>(); 
    // Find the TCP layer
    const TCP &tcp = pdu.rfind_pdu<TCP>(); 
    cout << ip.src_addr() << ':' << tcp.sport() << " -> " 
         << ip.dst_addr() << ':' << tcp.dport() << endl;
    return true;
}

void recv_function() {
	unsigned char buffer[1024];
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
		if (t->get_flag(TCP::SYN) && t->get_flag(TCP::ACK)) {
			int port = t->dport();
			tcs[port].state.fetch_add(1);
		} else if(t->get_flag(TCP::RST)) {
			int port = t->dport();
			tcs[port].state.store(0);
		}
	}
}

void send_function() {
	PacketSender sender;
	while(warm_up);
	while(run) {
		for (int i = MIN_PORT; i < MAX_PORT; i++) {
			switch(tcs[i].state) {
				case TCP_STATE_SYN:
					{
						IP pkt = IP(dst_ip) / TCP(dst_port,i);
						pkt.rfind_pdu<TCP>().set_flag(TCP::SYN,1);
						sender.send(pkt);
						tcs[i].state.fetch_add(1);
						tcs[i].timestamp = 0;
					}
					break;
				case TCP_STATE_RST:
					{
						total.fetch_add(1);
						IP pkt = IP(dst_ip) / TCP(dst_port,i);
						pkt.rfind_pdu<TCP>().set_flag(TCP::RST,1);
						sender.send(pkt);
						tcs[i].state.store(0); //fetch_add(1);
					}
					break;
				default:
				       	if(0) // TODO check time stamp
						tcs[i].state = 0;
					break;

			}
		}
	}
}

int main(int argc, char *argv[]) {

	if (argc != 3) {
		cout<< "Help: " << argv[0] << " ip_address port" << endl; 
		cout<< "Example: " << argv[0] << " 127.0.0.1 80" << endl; 
		return 0;	
	}

	strcpy(dst_ip, argv[1]);
	dst_port = stoi(argv[2]);

	for (int i = MIN_PORT; i < MAX_PORT; i++) {
		tcs[i].port = i;
	}
	std::thread recv_thread(recv_function);
	std::thread send_thread(send_function);

	sleep(1);
	// wait for warm up
	warm_up.store(0);
	

        sleep(10);

	// wait for run
	run.store(0);

	recv_thread.join();
	send_thread.join();
	
	cout << "stats" << total << endl;

	return 0;
}
