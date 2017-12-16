#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdexcept>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>
#include <memory>
#include <fstream>
#include <vector>
#include <sched.h>
#include <pthread.h>
#include <time.h>
#include <iomanip>

template<class T>
char* as_bytes(T& i)
{ 
  void* addr = &i;
  return static_cast<char *>(addr);
}


int main(int argc, char* argv[])
{
  
  static const int udp_port_number = 1414;
  static const int packet_size = 5032;
  static const int max_events = 300;
  //int num_nodes = atoi(argv[2]);
  int *nodes = new int[300];
 
  int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);      
  if (sock_fd < 0)
  {
    std::cout << "network thread: socket() failed: " << strerror(errno) << std::endl;
    //throw runtime_error(strerror(errno));
    exit(0);
  }
  
  struct sockaddr_in server_address;
  memset(&server_address, 0, sizeof(server_address));
  
  server_address.sin_family = AF_INET;
  inet_pton(AF_INET, argv[1], &server_address.sin_addr);
  server_address.sin_port = htons(udp_port_number);
  
  if (::bind(sock_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
  {
    std::cout << "network thread: bind() failed: " << strerror(errno) << std::endl;
    //throw runtime_error(strerror(errno));
    exit(0);
  }
  
  int n = 16*1024*1024;
  if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, (void *) &n, sizeof(n)) < 0)
  {
    std::cout << "network thread: setsockopt() failed: " << strerror(errno) << std::endl;
    //throw runtime_error(strerror(errno));
    exit(0);
  }
  
  struct epoll_event events[max_events];
  struct epoll_event ev;
  int epoll_fd = epoll_create(10);
  
  if (epoll_fd < 0)
  {
    std::cout << "network thread: epoll_create() failed: " << strerror(errno) << std::endl;
    //throw runtime_error(strerror(errno));
    exit(0);
  }
  
  ev.events = EPOLLIN;
  ev.data.fd = sock_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev) < 0)
  {
    std::cout << "network thread: epoll_ctl() failed: " << strerror(errno) << std::endl;
    //throw runtime_error(strerror(errno));
    exit(0);
  }
  //struct timeval tv0 = get_time();
    
  
  uint64_t total_events=0;  
  uint32_t *packet = new uint32_t[packet_size/4];
  
  std::chrono::system_clock::time_point start = std::chrono::high_resolution_clock::now();
  
  long int count=0,c=0;
  //std::ofstream fpout("data.dat");
  
  while (1)
  {
    int rack,node;
    uint32_t num_events = epoll_wait(epoll_fd, events, max_events, -1);
   
    if (num_events < 0)
    {
      std::cout << "network thread: epoll_wait() failed: " << strerror(errno) << std::endl;
      //throw runtime_error(strerror(errno));
      exit(0);
    }

      
    
    //std::cout<<"total_events: "<<total_events<< "  \n";   

    for (uint32_t i = 0; i < num_events; i++)
    {
      if (events[i].data.fd != sock_fd)
        continue;

      ssize_t bytes_read = read(sock_fd, packet, packet_size);

      if (bytes_read != packet_size)
      {
        std::cout<<"Something is wrong. Please check \n";
        std::cout.flush();
      }
      
    }
    total_events += num_events;
    int south=0; 
    if( packet[2]<512)
    {
      rack = packet[2]/40;
      node = 9-(packet[2]/4)%10;
    }
    else
    {
      rack = (packet[2]-512)/40;
      node = 9-((packet[2]-512)/4)%10;
      south =1;
    }
    
    nodes[south*128+rack*10+(9-node)] = 1;
    //nodes[packet[2]] = 1;
    if(rack>6) rack += 1;

    if(count/50000==c)
    {
      int dect_nodes = 0;
      for(int i=0;i<258;i++) dect_nodes+=nodes[i];
      if(dect_nodes==0) dect_nodes=1;
      uint64_t *fpga_counts;
      fpga_counts = reinterpret_cast<uint64_t*>(packet);
      std::cout<<"Beam-Ids: "<<packet[6]<<" Freq_id "<<packet[2];
      if(south==0)std::cout<<" Hut: North ";
      else std::cout<<" Hut: South ";
      std::cout<<" Rack: "<<rack<<" Node: "<<node<<" FPGA count:"<<fpga_counts[1];
      long nsec = std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::high_resolution_clock::now() - start).count();
      long double gbps = ((long double)(total_events*packet_size*8)/((long double)nsec));
      long double packet_loss = 100*(gbps-(dect_nodes*0.02516))/(dect_nodes*0.02516);  
      std::cout<<" detected nodes: "<<dect_nodes<<"  gbps: "<<gbps<<"  packet_loss: "<<std::setprecision(8) << packet_loss<<" % \n";
      
      
      for(int r=0;r<7;r++)
      {
        std::cout<<"Hut: North Rack: "<<r<<" ";
        for(int i=0;i<10;i++)
        {
          if(nodes[r*10+9-i]==1) std::cout<<i<<" ON ";
          else std::cout<<i<<" OF ";
        }
        std::cout<<"\n\n";
      }
      
      for(int r=8;r<13;r++)
      {
        std::cout<<"Hut: North Rack: "<<r<<" ";
        for(int i=0;i<10;i++)
        {
          if(nodes[(r-1)*10+9-i]==1) std::cout<<i<<" ON ";
          else std::cout<<i<<" OF ";
        }
        std::cout<<"\n\n";
 
      }     
      
      std::cout<<"Hut: North Rack: 13 ";
      for(int i=2;i<10;i++)
      {
        if(nodes[120+9-i]==1) std::cout<<i<<" ON ";
         else std::cout<<i<<" OF ";
      }

      std::cout<<"\n\n";
  
      for(int r=0;r<7;r++)
      {
        std::cout<<"Hut: South Rack: "<<r<<" ";
        for(int i=0;i<10;i++)
        {
          if(nodes[128+(r)*10+9-i]==1) std::cout<<i<<" ON ";
          else std::cout<<i<<" OF ";
        } 
        std::cout<<"\n\n";
      }
         
      for(int r=8;r<13;r++)
      {
        std::cout<<"Hut: South Rack: "<<r<<" ";
        for(int i=0;i<10;i++)
        {
          if(nodes[128+(r-1)*10+9-i]==1) std::cout<<i<<" ON ";
          else std::cout<<i<<" OF ";
        }
        std::cout<<"\n\n";

      }
      std::cout<<"Hut: South Rack: 13 ";
      for(int i=2;i<10;i++) 
      {
        if(nodes[248+9-i]==1) std::cout<<i<<" ON ";
         else std::cout<<i<<" OF ";
      }
      std::cout<<"\n\n";
      std::cout.flush();
      c++;
      
      //fpout<<data[46]<<"\n";
      //fpout.flush();
    }
    count++;
  }
  //fpout.close();
  return 0;
} 
