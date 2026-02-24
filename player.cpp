#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "potato.h"

using namespace std;


int main(int argc, char * argv[]){
    if (argc != 3) {
        cerr << "Usage: player <machine_name> <port_num>" << endl;
        return 1;
      }
    
      const char * machine_name = argv[1];// 为什么不用cpp中的string来存
      const char * port_str = argv[2];// Q:为什么这里不再用int来存


      //用getaddrinfo解析主机名成IP地址，比gethostbyname更安全更现代
      //Q: what is hint
      struct addrinfo hints, *result;
      memset(&hints, 0, sizeof(hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;

      int status = getaddrinfo(machine_name, port_str, &hints, &result);
      if(status != 0){
        cerr << "Error: getaddrinfo failed: " << gai_strerror(status) << endl;
        return 1;
      }

      //创建socket
      int master_fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
      if(master_fd < 0){
        cerr << "Error: socket creation failed" << endl;
      }

      //连接到ringmaster
      //connect()会主动去敲ringmaster的accept()的门
      if(connect(master_fd, result->ai_addr, result->ai_addrlen) < 0){
        cerr << "Error: connect failed" << endl;
        return 1;
      }

      freeaddrinfo(result);

      //从ringmaster接收信息
      //ringmaster发了两个int:player_id, num_players
      //这里recv顺序必须和ringmaster的send顺序一致
      int player_id;
      int num_players;
      recv(master_fd, &player_id, sizeof(player_id), MSG_WAITALL);
      recv(master_fd, &num_players, sizeof(num_players), MSG_WAITALL);

      int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
      if (listen_fd < 0) {
        cerr << "Error: listen socket creation failed" << endl;
        return 1;
      }

      int opt = 1;
      setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

      struct sockaddr_in my_addr;
      memset(&my_addr, 0, sizeof(my_addr));
      my_addr.sin_family = AF_INET;
      my_addr.sin_addr.s_addr = INADDR_ANY;
      my_addr.sin_port = htons(0); // 端口 0 = 让系统自动分配

      if (::bind(listen_fd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        cerr << "Error: bind failed" << endl;
        return 1;
      }

      // 用 getsockname 获取系统分配的端口号
      struct sockaddr_in bound_addr;
      socklen_t bound_len = sizeof(bound_addr);
      getsockname(listen_fd, (struct sockaddr *)&bound_addr, &bound_len);
      int my_listen_port = ntohs(bound_addr.sin_port);

      listen(listen_fd, 1);

      // 把自己的 hostname + 监听端口发给 ringmaster
      char my_hostname[256];
      memset(my_hostname, 0, sizeof(my_hostname));
      gethostname(my_hostname, sizeof(my_hostname));

      send(master_fd, my_hostname, sizeof(my_hostname), 0);
      send(master_fd, &my_listen_port, sizeof(my_listen_port), 0);

      char right_host[256];
      int right_port;
      recv(master_fd, right_host, sizeof(right_host), MSG_WAITALL);
      recv(master_fd, &right_port, sizeof(right_port), MSG_WAITALL);

      // 连接到右邻居（作为客户端）
      struct addrinfo hints2, *result2;
      memset(&hints2, 0, sizeof(hints2));
      hints2.ai_family = AF_INET;
      hints2.ai_socktype = SOCK_STREAM;

      char right_port_str[16];
      snprintf(right_port_str, sizeof(right_port_str), "%d", right_port);

      if (getaddrinfo(right_host, right_port_str, &hints2, &result2) != 0) {
        cerr << "Error: getaddrinfo for right neighbor failed" << endl;
        return 1;
      }

      int right_fd = socket(result2->ai_family, result2->ai_socktype, result2->ai_protocol);
      if (connect(right_fd, result2->ai_addr, result2->ai_addrlen) < 0) {
        cerr << "Error: connect to right neighbor failed" << endl;
        return 1;
      }
      freeaddrinfo(result2);

      // 接受左邻居的连接（作为服务器）
      struct sockaddr_in left_addr;
      socklen_t left_len = sizeof(left_addr);
      int left_fd = accept(listen_fd, (struct sockaddr *)&left_addr, &left_len);
      if (left_fd < 0) {
        cerr << "Error: accept left neighbor failed" << endl;
        return 1;
      }

      close(listen_fd); // 监听 socket 不再需要

      int left_id = (player_id - 1 + num_players) % num_players;
      int right_id = (player_id + 1) % num_players;

      cout << "Connected as player " << player_id << " out of " << num_players << " total players" << endl;

      srand((unsigned int)time(NULL) + player_id);

      while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(master_fd, &read_fds);
        FD_SET(left_fd, &read_fds);
        FD_SET(right_fd, &read_fds);
        
        int max_fd = master_fd;
        if (left_fd > max_fd) max_fd = left_fd;
        if (right_fd > max_fd) max_fd = right_fd;
        
        select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        
        Potato potato;
        int src_fd = -1;
        if (FD_ISSET(master_fd, &read_fds)) src_fd = master_fd;
        else if (FD_ISSET(left_fd, &read_fds)) src_fd = left_fd;
        else if (FD_ISSET(right_fd, &read_fds)) src_fd = right_fd;
        
        recv(src_fd, &potato, sizeof(potato), MSG_WAITALL);
        
        // hops == -1 是关闭信号
        if (potato.hops <= 0) break;
        
        potato.hops--;
        potato.trace[potato.count++] = player_id;
        
        if (potato.hops == 0) {
            cout << "I'm it" << endl;
            send(master_fd, &potato, sizeof(potato), 0);
        } else {
            int choice = rand() % 2; // 0=左, 1=右
            if (choice == 0) {
            cout << "Sending potato to " << left_id << endl;
            send(left_fd, &potato, sizeof(potato), 0);
            } else {
            cout << "Sending potato to " << right_id << endl;
            send(right_fd, &potato, sizeof(potato), 0);
            }
      }
    }

    

      close(left_fd);
      close(right_fd);
      close(master_fd);
      return 0;
}