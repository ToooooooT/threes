all:
	g++ -std=c++11 -O3 -g -Wall -fmessage-length=0 -o threes threes.cpp
stats:
	./threes --total=10000 --block=1000 --slide="init="65536,65536,65536,65536,65536,65536,65536,65536" alpha=0.0125 save=weights.bin load=weights.bin" --save="stats.txt"
clean:
	rm threes

