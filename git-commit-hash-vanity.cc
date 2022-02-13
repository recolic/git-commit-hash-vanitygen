#include "openssl/sha.h"
#include <string>
#include <chrono>
#include <thread>
#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>
#include <rlib/stdio.hpp>
#include <rlib/string.hpp>
#include <rlib/opt.hpp>
using namespace rlib::literals;

#define assertm(exp, msg) assert(((void)msg, exp))
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
using byteptr = unsigned char *;

void dump( const unsigned char *p, unsigned int n ) {
  for ( unsigned int i = 0; i < n; i++ ) {
     std::cerr << std::hex << (unsigned int)(p[i]) << " ";
  }
  std::cerr << std::endl;
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
 * | commit XXX \0 |            sprintf(fmt, args)                  |
 * ---------------------------------------------------------------
 * 0               11                                               64
 *                 --------------------------------------------------
 *                 | fmt  timestamp  fmt  timestamp   payload_msg   |
 *                 --------------------------------------------------
 */
void crack_thread(const std::string catfile_fmt, long timestamp_begin, long timestamp_end, const std::string commit_msg) {
    std::string databuf (catfile_fmt.size() + 64, 0); // hardcoded to 64, payload_msgbuf should be enough. 
    std::string payload_msg_buf (64 - 20 - 10, 0); // two timestamp takes 20 bytes. (should be 16, but who cares) "commit xxx" takes 10 bytes. 
    unsigned char hashbuf [20];

    auto catfile_result_size_log10 = (size_t)std::log10(catfile_fmt.size()); // Code error for speedup: assuming log10(fmt.size) == log10(result.size)
    assertm((size_t)std::log10(catfile_fmt.size() + 64) == catfile_result_size_log10, "If this failed, std::sprintf MAY fail. fmt.size < result.size < databuf.size");
    assertm(2 == catfile_result_size_log10, "If this failed, we must adjust the buf below.");
    const char _commitXXX_msgbuf[] = "commit XXX"; // Using assumption in previous line: size_log10 == 2
    const auto _commitXXX_msgbuf_size = sizeof(_commitXXX_msgbuf);
    
    for(long payload_msg_count = 0; ; ++payload_msg_count) {
        const auto payload_msg_size = gen_payload_msg(payload_msg_buf, payload_msg_count);
        for(long payload_ts_count_1 = timestamp_begin; payload_ts_count_1 < timestamp_end; ++payload_ts_count_1) {
            for(long payload_ts_count_2 = timestamp_begin; payload_ts_count_2 < timestamp_end; ++payload_ts_count_2) {
                // populate databuf. payload_msg_buf.data() would always have NULL-ending. 
                const auto body_size = std::sprintf(databuf.data() + _commitXXX_msgbuf_size, catfile_fmt.data(), payload_ts_count_1, payload_ts_count_2, payload_msg_buf.data());
                const auto head_size = std::sprintf(databuf.data(), "commit %lu", body_size) + 1; // plus tailing \0
#ifdef DEBUG    
                assertm(head_size == _commitXXX_msgbuf_size, "If this failed, assumption in line 59 failed.");
                assertm(body_size + head_size <= databuf.size(), "If this failed, memory will mess up.");
                assertm(body_size > 0, "If this failed, sprintf failed.");
#endif
                // databuf is ready. Calculate the SHA1
                SHA1((byteptr)databuf.data(), body_size + head_size, hashbuf);

                // Check if the hash is good
                if(unlikely(*(uint32_t *)hashbuf == 0)) {
                    // TODO: add more requirement for check-hash
                    auto outputbuf = new std::string(commit_msg.size() + 128, 0);
                    std::sprintf(outputbuf->data(), commit_msg.c_str(), payload_msg_buf.data());
                    std::printf("GIT_COMMITTER_DATE='%ld +0800' git commit -m '%s' --date '%ld +0800'\n", payload_ts_count_2, outputbuf->data(), payload_ts_count_1);
#ifdef DEBUG    
                    dump(hashbuf, 20);
                    databuf[head_size-1] = '|';
                    std::fprintf(stderr, "DEBUG: DATABUF >>>%s<<<, body_size=%ld\n", databuf.data(), body_size);
#endif
                    exit(0);
                }
            }
        }
        // No need to cleanup payload_msg_buf, because it only grows larger. Next call would overwrite it. 
        if(payload_msg_count % 0x100 == 0)
            std::fprintf(stderr, "Thread finished %ld operations\n", payload_msg_count * (timestamp_end - timestamp_begin) * (timestamp_end - timestamp_begin));
    }
}

int main(int argc, char **argv) {
    rlib::opt_parser args(argc, argv);
    if(args.getBoolArg("--help", "-h")) {
        rlib::printfln(std::cerr, "Usage: {} --msg 'Update doc %s' --tree ce75e3b2f81907d4481569d753b7351321553579 --parent 0000000acda3fd97f5880cf0834c9498e01e774e --author 'Recolic Keghart <root@recolic.net>' --timezone '+0800'", args.getSelf());
        return 1;
    }
    auto commit_msg = args.getValueArg("--msg", "-m");
    auto tree_hash = args.getValueArg("--tree", "-t");
    auto parent_hash = args.getValueArg("--parent", "-p");
    auto author = args.getValueArg("--author", "-a");
    auto timezone = args.getValueArg("--timezone", "-z", false, "+0800");
    if(commit_msg.find("%s") == std::string::npos) throw std::invalid_argument("commit_msg must contain exactly one '%s'. ");

    auto catfile_text = 
R"TXT(tree {}
parent {}
author {} %ld {}
committer {} %ld {}

)TXT"_format(tree_hash, parent_hash, author, timezone, author, timezone) + commit_msg + "\n";
    auto timestamp_center = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    auto thread_count = std::thread::hardware_concurrency();

    for(auto i = 0; i < thread_count; ++i) {
        std::thread(crack_thread, catfile_text, timestamp_center + 300*i, timestamp_center + 300*(i+1), commit_msg).detach();
    }
    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::hours(std::numeric_limits<int>::max()));
}

