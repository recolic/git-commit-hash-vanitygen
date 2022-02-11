#include "openssl/sha.h"
#include <string>
#include <cstdio>

void sha1_ossl (const unsigned char* data) {
    unsigned char  hash[20];
    for (long i=0; i<100000000; ++i) {
      SHA1(data, 200, hash);
    
      if (*(uint32_t *)hash==0)
          break;
    }
}

int main() {
    std::string s(200, 'a');
    sha1_ossl((unsigned char *)s.data());
    printf("Finished 100000000 SHA1, 1 thread, 200 bytes\n");
}

