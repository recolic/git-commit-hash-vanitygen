
build:
	g++ git-commit-hash-vanity.cc -lssl -lcrypto -lpthread -O3 -o git-commit-hash-vanity

bench:
	g++ sha1-benchmark.cc -lssl -lcrypto -O3 -o sha1-benchmark
	time ./sha1-benchmark


