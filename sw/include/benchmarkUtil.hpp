#pragma once

#include <iomanip>
#include <string>
#include <numeric>

#include "cProcess.hpp"
#include "dedupSys.hpp"

namespace dedup {

/* Def params */
constexpr auto const TARGET_REGION = 0;

struct Context {
  dedup::dedupSys &dedup_sys;
  fpga::cProcess &cproc;
  char *all_unique_page_buffer;
  uint32_t *all_unique_page_sha3;
  bool verbose;
  bool sync_on;
  int *goldenPgIsExec;
  int *goldenPgRefCount;
  int *goldenPgIdx;
  string &output_dir;
  stringstream &timeStamp;
};

stringstream buildTimeStamp() {
  time_t now = time(0);
  tm *ltm = localtime(&now);
  stringstream timeStamp;
  timeStamp << std::setfill ('0') << std::setw(4) << 1900 + ltm->tm_year << '_' ;
  timeStamp << std::setfill ('0') << std::setw(2) << 1 + ltm->tm_mon << std::setfill ('0') << std::setw(2) << ltm->tm_mday << '_' ;
  timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_hour;
  timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_min;
  timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_sec;
  return timeStamp;
}

/**
 * @brief Average it out
 * 
 */
double vctr_avg(std::vector<double> const& v) {
  return 1.0 * std::accumulate(v.begin(), v.end(), 0LL) / v.size();
}

uint32_t ceilPage(uint32_t byteCount) {
  return (byteCount + dedupSys::pg_size - 1) / dedupSys::pg_size;
}

uint32_t ceilHugePage(uint32_t byteCount) {
  return (byteCount + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
}

void updateGolden(Context &ctx, OpCode &instr, uint32_t pgIdx, uint32_t lba) {
  if (instr == READ) {
    ctx.goldenPgIsExec[pgIdx] = 1;
    ctx.goldenPgIdx[pgIdx] = 0;
  } else if (instr == WRITE) {
    ctx.goldenPgIsExec[pgIdx] = (ctx.goldenPgRefCount[pgIdx] > 0) ? 0 : 1;
    ctx.goldenPgRefCount[pgIdx] += 1;
    ctx.goldenPgIdx[pgIdx] = lba;
  } else {
    assert(ctx.goldenPgRefCount > 0);
    ctx.goldenPgIsExec[pgIdx] = (ctx.goldenPgRefCount[pgIdx] > 1) ? 0 : 1;
    ctx.goldenPgRefCount[pgIdx] -= 1;
    ctx.goldenPgIdx[pgIdx] = 0;
  }
}

bool modPages(Context &ctx, vector<Instr>::iterator instrs_begin, vector<Instr>::iterator instrs_end, stringstream &outfile_name, double &time, bool init_sha3 = false) {
  uint32_t pg_count = 0;
  uint32_t data_pg_count = 0; // How much page data actually has to be communicated
  uint32_t instr_count = 0;
  for (auto it = instrs_begin; it != instrs_end; ++it) {
    auto &instr = *it;
    auto instr_len = instr.pg_idx_lst.size();
    pg_count += instr_len;
    if (instr.opcode == WRITE) {
      data_pg_count += instr_len;
    }
    instr_count++;
  }
  uint32_t instr_pg_num = ceilPage(instr_count * dedupSys::instr_size); // 1 op
  uint32_t n_hugepage_req = ceilHugePage((data_pg_count + instr_pg_num) * dedupSys::pg_size); // roundup, number of hugepage for n page
  uint32_t n_hugepage_rsp = ceilHugePage(pg_count * 64); // roundup, number of huge page for 64B response from each page

  void *reqMem = ctx.cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_req});
  void *rspMem = ctx.cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_rsp});
  assert(reqMem != NULL);
  assert(rspMem != NULL);

  void* initPtr = reqMem;
  for (auto it = instrs_begin; it != instrs_end; ++it) {
    auto &instr = *it;
    if (instr.opcode == READ) {
      assert(instr.pg_idx_lst.size() == 1); 
      initPtr = set_read_instr(initPtr, ctx.all_unique_page_sha3 + instr.pg_idx_lst[0] * 8, false);
      updateGolden(ctx, instr.opcode, instr.pg_idx_lst[0], instr.lba);
    } else if (instr.opcode == WRITE) {
      // set instr: write instr
      initPtr = set_write_instr(initPtr, instr.lba, instr.pg_idx_lst.size(), false);

      // set pages
      char *initPtrChar = (char *) initPtr;
      for (int i = 0; i < instr.pg_idx_lst.size(); i ++) {
        size_t pgIdx = instr.pg_idx_lst[i];
        memcpy(initPtrChar, ctx.all_unique_page_buffer + pgIdx * dedupSys::pg_size, dedupSys::pg_size); // copy pages to request buffer
        updateGolden(ctx, instr.opcode, pgIdx, instr.lba + i);
        initPtrChar += dedupSys::pg_size;
      }
      initPtr = (void *) initPtrChar;
    } else {
      assert(instr.pg_idx_lst.size() == 1);
      initPtr = set_erase_instr(initPtr, ctx.all_unique_page_sha3 + instr.pg_idx_lst[0] * 8, false);
      updateGolden(ctx, instr.opcode, instr.pg_idx_lst[0], instr.lba);
    }
  }
  for (int nopIdx = 0; nopIdx < dedupSys::instr_per_page - (instr_count % dedupSys::instr_per_page); nopIdx++) {
    initPtr = set_nop(initPtr); // pad with nops to make page full. Otherwise, uninitialized random data could trigger instructions
  }
  
  ctx.verbose && (std::cout << "start execution" << endl);
  dedupSys::setReqStream(&ctx.cproc, reqMem, data_pg_count + instr_pg_num, rspMem);
  ctx.sync_on && ctx.dedup_sys.syncBarrier();

  auto begin_time = std::chrono::high_resolution_clock::now();
  dedupSys::setStart(&ctx.cproc);
  dedupSys::waitExecDone(&ctx.cproc, pg_count);
  auto end_time = std::chrono::high_resolution_clock::now();

  time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
  ctx.verbose && (std::cout << "time used: " << time << "ns" << std::endl);

  dedupSys::getExecDone(&ctx.cproc, data_pg_count + instr_pg_num, pg_count);

  /** parse and print the page response */
  ofstream outfile;
  if (ctx.verbose) {
    outfile.open(outfile_name.str(), ios::out);
  }

  ctx.verbose && (std::cout << "parsing the results" << endl);
  bool check_res = parse_response(instrs_begin, instrs_end, rspMem, ctx.goldenPgIsExec, ctx.goldenPgRefCount, ctx.goldenPgIdx, outfile);

  if (init_sha3) { // Copy SHA3 hashes from response to buffer
    uint32_t* rspMemUInt32 = (uint32_t*) rspMem;
    ctx.verbose && (std::cout << "get all SHA3" << endl);
    for (int i = 0; i < pg_count; i++) {
      memcpy(ctx.all_unique_page_sha3 + (size_t) instrs_begin->pg_idx_lst[i] * 8, (void*) (rspMemUInt32 + i * 16), 32); // This only works if there is only one write instruction for the init call
    }
  }

  if (ctx.verbose) {
    outfile.close();
  }

  ctx.cproc.freeMem(reqMem);
  ctx.cproc.freeMem(rspMem);

  std::cout << "all page passed?: " << (check_res ? "True" : "False") << endl;
  return check_res;
}

bool modPages(Context &ctx, OpCode opcode, uint32_t instr_count, uint32_t lba_offset, vector<uint32_t> &pg_idx_lst, stringstream &outfile_name, double &time, bool init_sha3 = false) {
  assert(instr_count > 0);
  assert(pg_idx_lst.size() % instr_count == 0);
  uint32_t instr_pg_count = pg_idx_lst.size() / instr_count;
  vector<Instr> instrs(instr_count, Instr{opcode, 0, vector<uint32_t>(instr_pg_count)});
  for (size_t i = 0; i < instr_count; i++) {
    auto start = instr_pg_count * i;
    instrs[i].lba = lba_offset + start;
    copy(pg_idx_lst.begin() + start, pg_idx_lst.begin() + start + instr_pg_count, instrs[i].pg_idx_lst.begin());
  }
  return modPages(ctx, instrs.begin(), instrs.end(), outfile_name, time, init_sha3);
}

bool modPages(Context &ctx, OpCode instr, uint32_t instr_count, uint32_t lba_offset, uint32_t pg_idx_start, uint32_t pg_count, stringstream &outfile_name, double &time, bool init_sha3 = false) {
  vector<uint32_t> pg_idx_lst;
  for (size_t i = 0; i < pg_count; i++) {
    pg_idx_lst.push_back(pg_idx_start + i);
  }
  return modPages(ctx, instr, instr_count, lba_offset + pg_idx_start, pg_idx_lst, outfile_name, time, init_sha3);
}

void initPages(Context &ctx, uint32_t initial_page_unique_count, uint32_t new_page_unique_count) {
  // support 64x2M page address mapping, do 32 in one insertion round
  auto total_page_unique_count = initial_page_unique_count + new_page_unique_count;
  uint32_t n_insertion_round = (total_page_unique_count + 32 * dedupSys::pg_per_huge_pg - 1) / (32 * dedupSys::pg_per_huge_pg);
  std::cout << "insert all unique pages, in "<< n_insertion_round << " rounds"<< endl;

  for (int insertion_round_idx = 0; insertion_round_idx < n_insertion_round; insertion_round_idx++) {
    uint32_t pg_idx_start = insertion_round_idx * 32 * dedupSys::pg_per_huge_pg;
    uint32_t pg_idx_end = ((pg_idx_start + 32 * dedupSys::pg_per_huge_pg) > total_page_unique_count) ? total_page_unique_count : (pg_idx_start + 32 * dedupSys::pg_per_huge_pg);
    uint32_t pg_idx_count = pg_idx_end - pg_idx_start;
    stringstream outfile_name;
    outfile_name << ctx.output_dir << "/resp_" << ctx.timeStamp.str() << "_step1.txt";
    std::cout << "round " << insertion_round_idx << endl;

    double time;
    modPages(ctx, OpCode::WRITE, 1, 100, pg_idx_start, pg_idx_count, outfile_name, time, true); // This only works if there is only one write instruction for the init call
  }

  std::cout << endl << "Step2: clean new pages" << endl;
  if (new_page_unique_count > 0) {
    std::stringstream outfile_name;
    outfile_name << ctx.output_dir << "/resp_"<< ctx.timeStamp.str() << "_step2.txt";

    double time;
    modPages(ctx, OpCode::ERASE, new_page_unique_count, 0, initial_page_unique_count, new_page_unique_count, outfile_name, time);
  } else {
    std::cout << "Nothing to clean" << endl;
  }
}

bool initSHA3(Context &ctx, size_t total_page_unique_count) {
  // support 64x2M page address mapping, do 32 in one insertion round
  size_t n_insertion_round = (total_page_unique_count + 32 * dedupSys::pg_per_huge_pg - 1) / (32 * dedupSys::pg_per_huge_pg);
  std::cout << "insert all unique pages, in "<< n_insertion_round << " rounds"<< endl;

  for (int insertion_round_idx = 0; insertion_round_idx < n_insertion_round; insertion_round_idx++) {
    size_t pg_idx_start = insertion_round_idx * 32 * dedupSys::pg_per_huge_pg;
    size_t pg_idx_end = ((pg_idx_start + 32 * dedupSys::pg_per_huge_pg) > total_page_unique_count) ? total_page_unique_count : (pg_idx_start + 32 * dedupSys::pg_per_huge_pg);
    size_t pg_idx_count = pg_idx_end - pg_idx_start;
    stringstream outfile_name;
    outfile_name << ctx.output_dir << "/resp_" << ctx.timeStamp.str() << "_step1.txt";
    std::cout << "round " << insertion_round_idx << endl;

    double time;
    auto check_res = modPages(ctx, OpCode::WRITE, 1, 100, pg_idx_start, pg_idx_count, outfile_name, time, true); // This only works if there is only one write instruction for the init call
    if (!check_res) return false;
    check_res = modPages(ctx, OpCode::ERASE, pg_idx_count, 0, pg_idx_start, pg_idx_count, outfile_name, time);
    if (!check_res) return false;
  }
  return true;
}

void readFile(string filename, char *buffer, size_t size) {
  int fd;
  if ((fd = open(filename.c_str(), O_RDONLY)) < 0) {
      cout << "Not possible to open input page file" << endl;
  }
  ssize_t status = 0;
  off_t bytes_read = 0;
  while (bytes_read < size && (status = read(fd, buffer + bytes_read, size - bytes_read)) > 0) {
    bytes_read += status;
  }
  close(fd);
  if (status < 0 || bytes_read < size) {
    cout << "Error reading input page file with status " << status << " and " << bytes_read << " Bytes read out of " << size << " Bytes" << endl;
  }
}

}