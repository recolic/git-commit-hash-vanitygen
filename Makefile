CXX ?= g++

build:
	$(CXX) git-commit-hash-vanity.cc -lssl -lcrypto -lpthread -O3 -o git-commit-hash-vanity

bench:
	$(CXX) sha1-benchmark.cc -lssl -lcrypto -O3 -o sha1-benchmark
	time ./sha1-benchmark

install: build
	cp git-commit-hash-vanity /usr/bin/
	cp vgitcommit /usr/bin/

