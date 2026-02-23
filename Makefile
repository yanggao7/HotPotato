//理解这个makefile文件
all: ringmaster plyaer

ringmaster: ringmaster.cpp potato.h
	g++ -o ringmaster ringmaster.cpp