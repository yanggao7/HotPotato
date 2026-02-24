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
    for(int i = 0; i < num_players; i++){
        struct sockaddr_in player_addr;
        socklen_t addr_len = sizeof(player_addr);

        //Q:阻塞等待一个player连接体现在哪里？在for循环的框架下面必须一个一个连接吗
        player_fds[i] = accept(server_fd, (struct sockaddr *)&player_addr, &addr_len);
        if(player_fds[i] < 0){
            cerr << "Error: accept failed" << endl;
        }

        //把player的ID和总人数发给这个player
        int player_id = i;
        //send第一个参数：目标socket_fd，第二个：数据的指针，可以发任何类型，只要接收方知道格式；第三个：数据大小（字节数）；第四个flags,通常传0
        send(player_fds[i], &player_id, sizeof(player_id), 0);
        send(player_fds[i], &num_players, sizeof(num_players), 0);

        cout << "Player " << i << " is ready to play" << endl;
    }

    //TODO:环形连接、发potato等

    //清理：关闭所有socket
    for(int i = 0; i < num_players; i++){
        close(player_fds[i]);
    }
    close(server_fd);

    return 0;
}