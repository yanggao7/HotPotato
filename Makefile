# TODO: 理解这个makefile文件
all: ringmaster player

ringmaster: ringmaster.cpp potato.h
	g++ -o ringmaster ringmaster.cpp

player: player.cpp potato.h
	g++ -o player player.cpp

clean:
	rm -f ringmaster player
	