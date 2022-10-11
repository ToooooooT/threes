all:
	g++ -std=c++11 -O3 -g -Wall -fmessage-length=0 -o threes threes.cpp
load_stats:
	./threes --total=22000 --block=1000 --slide="alpha=0.25 save=weights.bin load=weights.bin" --save="stats.txt"
statss:
	./threes --total=22000 --block=1000 --slide="init="16777216,16777216,16777216,16777216," alpha=0.25 save=weights.bin" --save="stats.txt"
clean:
	rm threes

