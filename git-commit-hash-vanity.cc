#include "openssl/sha.h"
#include <string>
#include <chrono>
#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>
using namespace std::literals;

#define assertm(exp, msg) assert(((void)msg, exp))
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
using byteptr = unsigned char *;

void dump( const unsigned char *p, unsigned int n ) {
  for ( unsigned int i = 0; i < n; i++ ) {
     std::cout << std::hex << (unsigned int)(p[i]) << " ";
  }
  std::cout << std::endl;
}


size_t gen_payload_msg(std::string &outputbuf, long count) {
    // Assuming outputbuf is large enough. 
    constexpr auto alphabet_begin = ' ';
    constexpr auto alphabet_size = '~' - ' ';

    auto output_len = 0;
    while(count != 0) {
        outputbuf[output_len] = count % alphabet_size + alphabet_begin;
        ++output_len;
        count /= alphabet_size;
#ifdef DEBUG
        assertm(count >= 0, "if failed, this is code error.");
        assertm(output_len < outputbuf.size(), "if failed, increase payload_msg_buf.size.");
#endif
    }
    // outputbuf should be reversed, but it doesn't matter... We just generate a unique one and it's ok. 
    return output_len;
}

/*
 * databuf:
 * ---------------------------------------------------------------
 * | commit XXX |            sprintf(fmt, args)                  |
 * ---------------------------------------------------------------
 * 0            10                                               64
 *              --------------------------------------------------
 *              | fmt  timestamp  fmt  timestamp   payload_msg   |
 *              --------------------------------------------------
 */
void crack_thread(const std::string catfile_fmt, long timestamp_begin, long timestamp_end, const std::string commit_msg) {
    std::string databuf (catfile_fmt.size() + 64, 0); // hardcoded to 64, payload_msgbuf should be enough. 
    std::string payload_msg_buf (64 - 20 - 10, 0); // two timestamp takes 20 bytes. (should be 16, but who cares) "commit xxx" takes 10 bytes. 
    unsigned char hashbuf [20];

    auto catfile_result_size_log10 = (size_t)std::log10(catfile_fmt.size()); // Code error for speedup: assuming log10(fmt.size) == log10(result.size)
    assertm((size_t)std::log10(catfile_fmt.size() + 64) == catfile_result_size_log10, "If this failed, std::sprintf MAY fail. fmt.size < result.size < databuf.size");
    assertm(2 == catfile_result_size_log10, "If this failed, we must adjust the buf below.");
    const char _commitXXX_msgbuf[] = "commit XXX";
    std::memcpy(databuf.data(), _commitXXX_msgbuf, sizeof(_commitXXX_msgbuf));
    
    for(long payload_msg_count = 0; ; ++payload_msg_count) {
        auto payload_msg_size = gen_payload_msg(payload_msg_buf, payload_msg_count);
        for(long payload_ts_count = timestamp_begin; payload_ts_count < timestamp_end; ++payload_ts_count) {
            // populate databuf. payload_msg_buf.data() would always have NULL-ending. 
            auto ns_size = std::sprintf(databuf.data() + sizeof(_commitXXX_msgbuf), catfile_fmt.data(), payload_ts_count, payload_ts_count, payload_msg_buf.data());
#ifdef DEBUG
            assertm(ns_size + sizeof(_commitXXX_msgbuf) <= databuf.size(), "If this failed, memory will mess up.");
            assertm(ns_size > 0, "If this failed, sprintf failed.");
#endif
            SHA1((byteptr)databuf.data(), ns_size + sizeof(_commitXXX_msgbuf), hashbuf);
            if(unlikely(*(uint16_t *)hashbuf == 0)) {
                // TODO: add more requirement for check-hash
                auto outputbuf = new std::string(commit_msg.size() + 128, 0);
                std::sprintf(outputbuf->data(), commit_msg.c_str(), payload_msg_buf.data());
                std::printf("Found answer: GIT_COMMITTER_DATE='%ld +0800' git commit -m '%s' --date '%ld +0800', hash: \n", payload_ts_count, outputbuf->data(), payload_ts_count);
                dump(hashbuf, 20);
                // TODO: debug msg to show hashed databuf
                exit(0);
            }
        }
        // No need to cleanup payload_msg_buf, because it only grows larger. Next call would overwrite it. 
        if(payload_msg_count % 0x8000 == 0)
            std::printf("Thread finished %ld operations\n", payload_msg_count * (timestamp_end - timestamp_begin));
    }
}

int main() {
    auto commit_msg = "Update doc %s";
    auto catfile_text = 
R"TXT(tree 02b900fadfdd40d74c23a3f7fd943e4fb15fdca9
parent f8a6d039e7647ff54e6cf5a2d109ddf5c041ea86
author Recolic K <bensl@microsoft.com> %ld +0800
committer Recolic K <bensl@microsoft.com> %ld +0800

)TXT"s + commit_msg + "\n";
    // This program may generate git commit within the following time range. 
    auto timestamp_center = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    crack_thread(catfile_text, timestamp_center - 300, timestamp_center + 300, commit_msg);
}

