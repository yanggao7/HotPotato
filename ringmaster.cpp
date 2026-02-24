#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <unistd.h> // ?

//?
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "potato.h"

using namespace std;

int main(int argc, char * argv[]){


    if(argc != 4){

        //cerr是什么意思
        cerr<< "Uasge: ringmaster <port_num> <num_players> <num_hops>" << endl;
        return 1;
    }

    int port_num = atoi(argv[1]);
    int num_players = atoi(argv[2]);
    int num_hops = atoi(argv[3]);

    if (port_num <= 0 || port_num > 65535) {
        std::cerr << "Error: port_num must be between 1 and 65535" << std::endl;
        return 1;
    }
    if (num_players <= 1) {
        std::cerr << "Error: num_players must be greater than 1" << std::endl;
        return 1;
    }
      if (num_hops < 0 || num_hops > 512) {
        std::cerr << "Error: num_hops must be between 0 and 512" << std::endl;
        return 1;
    }

    std::cout << "Potato Ringmaster" << std::endl;
    std::cout << "Players = " << num_players << std::endl;
    std::cout << "Hops = " << num_hops << std::endl;


    //创建socket
    //Q：这里创建了个啥，socket不是地址加端口吗
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        std::cerr << "Error: socket creation failed" << std::endl;
        //Q：WHY RETURN 1
        return 1;
    }

    //Q:为什么能允许端口复用
    //A：当关闭程序后立刻重新运行，操作系统可能还没释放端口（TCP的TIME_WAIT状态）。
    //设置SO_REUSEDADDR允许立即重新绑定同一端口，避免Address already in use的报错
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    //绑定到指定端口
    //Q:这是个什么结构体
    struct sockaddr_in server_addr;
    //Q:memset是什么意思
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_num);


    if(::bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        cerr << "Error: bind failed: " << strerror(errno) << endl;
        return 1;
    }

    //开始监听
    if(listen(server_fd, num_players) < 0){
        cerr << "Error: listen failed" << endl;
        return 1;
    }

    //用数组保存每个player的socket fd
    int player_fds[num_players];
    char player_hosts[num_players][256];
    int player_ports[num_players];

    for(int i = 0; i < num_players; i++){
        struct sockaddr_in player_addr;
        socklen_t addr_len = sizeof(player_addr);

        //Q:阻塞等待一个player连接体现在哪里？在for循环的框架下面必须一个一个连接吗
        player_fds[i] = accept(server_fd, (struct sockaddr *)&player_addr, &addr_len);
        if(player_fds[i] < 0){
            cerr << "Error: accept failed" << endl;
            return 1;
        }

        int player_id = i;
        //send第一个参数：目标socket_fd，第二个：数据的指针，可以发任何类型，只要接收方知道格式；第三个：数据大小（字节数）；第四个flags,通常传0
        send(player_fds[i], &player_id, sizeof(player_id), 0);
        send(player_fds[i], &num_players, sizeof(num_players), 0);

        // 接收 player 的 hostname 和监听端口
        recv(player_fds[i], player_hosts[i], sizeof(player_hosts[i]), MSG_WAITALL);
        recv(player_fds[i], &player_ports[i], sizeof(player_ports[i]), MSG_WAITALL);

        cout << "Player " << i << " is ready to play" << endl;
    }

    // 把每个 player 的右邻居信息发给它
    // player i 的右邻居是 player (i+1) % N
    for(int i = 0; i < num_players; i++){
        int right = (i + 1) % num_players;
        send(player_fds[i], player_hosts[right], sizeof(player_hosts[right]), 0);
        send(player_fds[i], &player_ports[right], sizeof(player_ports[right]), 0);
    }

    //传递土豆儿

    srand((unsigned int)time(NULL));

    if(num_hops == 0){
        Potato shutdown;
        shutdown.hops = -1;
        for(int i = 0; i < num_players; i++){
            send(player_fds[i], & shutdown, sizeof(shutdown), 0);
        }
    }else{
        Potato potato;
        memset(&potato, 0, sizeof(potato));
        potato.hops = num_hops;
        potato.count = 0;


        int first = rand() % num_players;
        cout << "Ready to start the game, sending potato to player " << first << endl;
        send(player_fds[first], &potato, sizeof(potato), 0);

        //select()等待某个player把potato发回来
        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = 0;
        for(int i = 0; i < num_players; i++){
            FD_SET(player_fds[i], &read_fds);
            if(player_fds[i] > max_fd) max_fd = player_fds[i];
        }
        select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        //从发回potato的player那里收potato
        for(int i = 0; i < num_players; i++){
            if (FD_ISSET(player_fds[i], &read_fds)) {
                recv(player_fds[i], &potato, sizeof(potato), MSG_WAITALL);
                break;
            }
        }

        //print trace
        cout << "Trace of potato:" << endl;
        for (int i = 0; i < potato.count; i++) {
          if (i > 0) cout << ",";
          cout << potato.trace[i];
        }
        cout << endl;


        Potato shutdown;
        shutdown.hops = -1;
        for (int i = 0; i < num_players; i++) {
            send(player_fds[i], &shutdown, sizeof(shutdown), 0);
        }
    }


    
    for(int i = 0; i < num_players; i++){
        close(player_fds[i]);
    }
    close(server_fd);

    return 0;
}