#ifndef POTATO_H
#define POTATO_H


// MAX_HOPS 目前看是自定义的
#define MAX_HOPS 512

struct Potato{
    int hops; // 剩余跳数
    int count;  // 当前trace中有多少条记录
    int trace[MAX_HOPS]; // 经过的玩家ID轨迹
};

// 用固定大小的strcut方便通过socket发送/接收
#endif