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

      cout << "Connected as player " << player_id << " out of " << num_players << " total players" << endl;

      //TODO: 后续实现 建立环形连接、游戏循环等


      close(master_fd);
      return 0;
}