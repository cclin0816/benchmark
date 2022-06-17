#include <fcntl.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>

using namespace std::literals;

#include "../benchmark.h"

#define KiB 1024
#define MiB (KiB * KiB)
#define GiB (KiB * KiB * KiB)

const char src_file[] = "./tmpfs/src";
const char dst_file[] = "./tmpfs/dst";

uint64_t get_file_sz(int fd) {
  struct stat st;
  fstat(fd, &st);
  return st.st_size;
}

void pre_cp(const char *name) {
  std::cout << name << std::endl;
  if (system("rm -f ./tmpfs/dst && sync && echo 3 | sudo "
             "tee /proc/sys/vm/drop_caches 2>&1 > /dev/null") != 0) {
    std::cerr << "failed to clear caches" << std::endl;
    exit(1);
  }
}

void post_cp() {
  if (system("diff ./tmpfs/src ./tmpfs/dst") != 0) {
    std::cerr << "diff failed" << std::endl;
    exit(1);
  }
}

void cp_cp() {
  if (system("cp ./tmpfs/src ./tmpfs/dst") != 0) {
    std::cerr << "cp failed" << std::endl;
    exit(1);
  }
}

template <uint64_t Buf_Sz>
void buf_cp() {
  char *buf = new char[Buf_Sz];
  int fd_in = open(src_file, O_RDONLY);
  int fd_out = open(dst_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd_in < 0 || fd_out < 0) {
    std::cerr << "open failed" << std::endl;
    exit(1);
  }

  long read_len = 0;
  while (true) {
    read_len = read(fd_in, buf, Buf_Sz);
    if (read_len < 0) {
      std::cerr << "read file error" << std::endl;
      exit(1);
    }
    if (read_len == 0) {
      break;
    }
    while (read_len > 0) {
      auto write_len = write(fd_out, buf, (unsigned long)read_len);
      if (write_len < 0) {
        std::cerr << "write file error" << std::endl;
        exit(1);
      }
      read_len -= write_len;
    }
  }
  close(fd_in);
  close(fd_out);
  delete[] buf;
}

void mmap_cp() {
  int fd_in = open(src_file, O_RDONLY);
  int fd_out = open(dst_file, O_RDWR | O_CREAT, 0644);
  if (fd_in < 0 || fd_out < 0) {
    std::cerr << "open file error" << std::endl;
    exit(1);
  }

  auto size = get_file_sz(fd_in);
  if (ftruncate(fd_out, size) != 0) {
    std::cerr << "ftruncate file error" << std::endl;
    exit(1);
  }
  auto in_buf = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd_in, 0);
  auto out_buf = mmap(nullptr, size, PROT_WRITE, MAP_SHARED, fd_out, 0);

  if (in_buf == MAP_FAILED || out_buf == MAP_FAILED) {
    std::cerr << "mmap file error" << std::endl;
    exit(1);
  }

  memcpy(out_buf, in_buf, size);
  munmap(in_buf, size);
  munmap(out_buf, size);
  close(fd_in);
  close(fd_out);
}

void sendfile_cp() {
  int fd_in = open(src_file, O_RDONLY);
  int fd_out = open(dst_file, O_WRONLY | O_CREAT, 0644);
  if (fd_in < 0 || fd_out < 0) {
    std::cerr << "open file error" << std::endl;
    exit(1);
  }

  auto size = get_file_sz(fd_in);
  if (ftruncate(fd_out, size) != 0) {
    std::cerr << "ftruncate file error" << std::endl;
    exit(1);
  }
  sendfile(fd_out, fd_in, nullptr, size);
  close(fd_in);
  close(fd_out);
}

void copy_file_range_cp() {
  int fd_in = open(src_file, O_RDONLY);
  int fd_out = open(dst_file, O_WRONLY | O_CREAT, 0644);
  if (fd_in < 0 || fd_out < 0) {
    std::cerr << "open file error" << std::endl;
    exit(1);
  }

  auto size = get_file_sz(fd_in);
  if (ftruncate(fd_out, size) != 0) {
    std::cerr << "ftruncate file error" << std::endl;
    exit(1);
  }
  copy_file_range(fd_in, nullptr, fd_out, nullptr, size, 0);
  close(fd_in);
  close(fd_out);
}

void gen_rd_src(uint64_t size) {
  if (system(("dd if=/dev/urandom of=./tmpfs/src iflag=fullblock bs="s +
              std::to_string(size) + " count=1  2>&1 > /dev/null")
                 .c_str()) != 0) {
    std::cerr << "failed to generate src file" << std::endl;
    exit(1);
  }
}

uint64_t powul(const uint64_t base, uint64_t exp) {
  auto res = 1UL;
  auto cur = base;
  while (exp != 0) {
    if (exp & 1UL) {
      res *= cur;
    }
    cur *= cur;
    exp >>= 1;
  }
  return res;
}

int main() {
  auto buf_1k =
      bm::real_time(buf_cp<1 * KiB>, std::bind(pre_cp, "buf_1k_cp"), post_cp);
  auto buf_32k =
      bm::real_time(buf_cp<32 * KiB>, std::bind(pre_cp, "buf_32k_cp"), post_cp);
  auto buf_1m =
      bm::real_time(buf_cp<1 * MiB>, std::bind(pre_cp, "buf_1m_cp"), post_cp);
  auto buf_32m =
      bm::real_time(buf_cp<32 * MiB>, std::bind(pre_cp, "buf_32m_cp"), post_cp);
  auto buf_1g =
      bm::real_time(buf_cp<1 * GiB>, std::bind(pre_cp, "buf_1g_cp"), post_cp);
  auto mmap_bench =
      bm::real_time(mmap_cp, std::bind(pre_cp, "mmap_cp"), post_cp);
  auto sendfile_bench =
      bm::real_time(sendfile_cp, std::bind(pre_cp, "sendfile_cp"), post_cp);
  auto copy_file_range_bench = bm::real_time(
      copy_file_range_cp, std::bind(pre_cp, "copy_file_range_cp"), post_cp);
  auto cp_bench = bm::real_time(cp_cp, std::bind(pre_cp, "cp_cp"), post_cp);

  std::ofstream ofs("benchmark.csv");
  ofs << ",buf_1k_cp,buf_32k_cp,buf_1m_cp,buf_32m_cp,buf_1g_cp,mmap_cp,"
         "sendfile_cp,copy_file_range_cp,cp_cp"
      << std::endl;
  for (auto i = 0UL; i < 5UL; i++) {
    auto src_size = powul(32, i) * 1024;
    gen_rd_src(src_size);

    ofs << src_size << std::flush;
    auto res = bm::bench(5UL, bm::excl_avg<bm::nanos, 1>, buf_1k, buf_32k,
                         buf_1m, buf_32m, buf_1g, mmap_bench, sendfile_bench,
                         copy_file_range_bench, cp_bench);
    for (auto &r : res) {
      ofs << ',' << r.count();
    }
    ofs << std::endl;
  }
}