
build:
	g++ git-commit-hash-vanity.cc -lssl -lcrypto -lpthread -O3 -o git-commit-hash-vanity
	g++ sha1-benchmark.cc -lssl -lcrypto -O3 -o sha1-benchmark

bench: build
	time ./sha1-benchmark

run: build
	./git-commit-hash-vanity


