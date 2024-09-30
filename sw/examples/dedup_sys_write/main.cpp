#include <iostream>
#include <string>
#include <stdlib.h>
#include <time.h> 
#include <sys/time.h>
#include <sys/stat.h>
#include <chrono>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <vector>
#include <filesystem>

#ifdef EN_AVX
#include <x86intrin.h>
#endif
#include <boost/program_options.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <numeric>
#include <stdlib.h>
#include <cassert>

#include "cBench.hpp"
#include "cProcess.hpp"
#include "dedupUtil.hpp"
#include "dedupSys.hpp"

using namespace std;
using namespace fpga;
using namespace dedup;

/* Def params */
constexpr auto const TARGET_REGION = 0;

enum Instr {
  WRITE,
  ERASE
};

struct Context {
  dedupSys &dedup_sys;
  cProcess &cproc;
  char *all_unique_page_buffer;
  uint32_t *all_unique_page_sha3;
  bool verbose;
  bool sync_on;
  int *goldenPgIsExec;
  int *goldenPgRefCount;
  int *goldenPgIdx;
};

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

uint32_t ceilHugePage(uint32_t pageCount) {
  return (pageCount + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
}

void updateGolden(Context &ctx, Instr &instr, uint32_t pgIdx, uint32_t lba) {
  ctx.goldenPgIsExec[pgIdx] = (ctx.goldenPgRefCount[pgIdx] > 0) ? 0 : 1;
  if (instr == WRITE) {
    ctx.goldenPgRefCount[pgIdx] += 1;
    ctx.goldenPgIdx[pgIdx] = lba;
  } else {
    assert(ctx.goldenPgRefCount > 0);
    ctx.goldenPgRefCount[pgIdx] -= 1;
    ctx.goldenPgIdx[pgIdx] = 0;
  }
}

bool modPages(Context &ctx, Instr instr, uint32_t instr_count, uint32_t lba_offset, vector<uint32_t> &pg_idx_lst, stringstream &outfile_name, double &time) {
  uint32_t pg_count = pg_idx_lst.size();
  uint32_t data_pg_count = (instr == WRITE) ? pg_count : 0; // How much page data actually has to be communicated
  uint32_t instr_pg_num = ceilPage(instr_count * dedupSys::instr_size); // 1 op
  uint32_t n_hugepage_req = ceilHugePage(data_pg_count + instr_pg_num); // roundup, number of hugepage for n page
  uint32_t n_hugepage_rsp = ceilHugePage(pg_count * 64); // roundup, number of huge page for 64B response from each page

  void* reqMem = ctx.cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_req});
  void* rspMem = ctx.cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_rsp});

  void* initPtr = reqMem;
  for (int instrIdx = 0; instrIdx < dedupSys::instr_per_page * instr_pg_num; instrIdx ++) {
    if (instrIdx < instr_count) {
      if (instr == WRITE) {
        uint32_t insert_page_per_instr = pg_count / instr_count;
        uint32_t curr_pg_start = instrIdx * insert_page_per_instr;
        // set instr: write instr
        initPtr = set_write_instr(initPtr, lba_offset + curr_pg_start, insert_page_per_instr, false);

        // set pages
        char* initPtrChar = (char*) initPtr;
        for (int i = 0; i < insert_page_per_instr; i ++) {
          int pgIdx = pg_idx_lst[instrIdx * insert_page_per_instr + i];
          memcpy(initPtrChar + i * dedupSys::pg_size, ctx.all_unique_page_buffer + pgIdx * dedupSys::pg_size, dedupSys::pg_size); // copy pages to request buffer
        }
        initPtrChar = initPtrChar + insert_page_per_instr * dedupSys::pg_size;
        initPtr = (void*) initPtrChar;
      } else {
        initPtr = set_erase_instr(initPtr, ctx.all_unique_page_sha3 + pg_idx_lst[instrIdx] * 8, false);
      }
    } else {
      // set Insrt: nop
      initPtr = set_nop(initPtr); // pad with nops to make page full. Otherwise, uninitialized random data could trigger instructions
    }
  }

  for (int i = 0; i < pg_count; i ++){
    updateGolden(ctx, instr, pg_idx_lst[i], lba_offset + i);
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
  bool check_res = parse_response(pg_count, rspMem, ctx.goldenPgIsExec, ctx.goldenPgRefCount, ctx.goldenPgIdx, (instr == WRITE) ? 1 : 2, outfile);

  if (instr == WRITE) { // Copy SHA3 hashes from response to buffer
    uint32_t* rspMemUInt32 = (uint32_t*) rspMem;
    ctx.verbose && (std::cout << "get all SHA3" << endl);
    for (int i=0; i < pg_count; i++) {
      memcpy(ctx.all_unique_page_sha3 + pg_idx_lst[i] * 8, (void*) (rspMemUInt32 + i * 16), 32);
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

bool modPages(Context &ctx, Instr instr, uint32_t instr_count, uint32_t lba_offset, uint32_t pg_idx_start, uint32_t pg_count, stringstream &outfile_name, double &time) {
  vector<uint32_t> pg_idx_lst;
  for (size_t i = 0; i < pg_count; i++) {
    pg_idx_lst.push_back(pg_idx_start + i);
  }
  return modPages(ctx, instr, instr_count, lba_offset + pg_idx_start, pg_idx_lst, outfile_name, time);
}

/**
 * @brief Throughput and latency tests, read and write
 * 
 */
int main(int argc, char *argv[])
{   
  time_t now = time(0);
  tm *ltm = localtime(&now);
  stringstream timeStamp;
  timeStamp << std::setfill ('0') << std::setw(4) << 1900 + ltm->tm_year << '_' ;
  timeStamp << std::setfill ('0') << std::setw(2) << 1 + ltm->tm_mon << std::setfill ('0') << std::setw(2) << ltm->tm_mday << '_' ;
  timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_hour;
  timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_min;
  timeStamp << std::setfill ('0') << std::setw(2) << ltm->tm_sec;

  // ---------------------------------------------------------------
  // Args 
  // ---------------------------------------------------------------
  boost::program_options::options_description programDescription("Options:");
  programDescription.add_options()
  ("nPage,n", boost::program_options::value<uint32_t>()->default_value(1024), "Number of Pages in 1 Batch")
  ("dupRatio,d", boost::program_options::value<double>()->default_value(0.5), "Overlap(batch, existing page)/nPage")
  ("fullness,f", boost::program_options::value<double>()->default_value(0.5), "fullness of hash table")
  ("throuFactor,t", boost::program_options::value<uint32_t>()->default_value(8), "Store throughput factor")
  ("writeOpNum,w", boost::program_options::value<uint32_t>()->default_value(8), "Write op Num")
  ("nBenchRun,r", boost::program_options::value<uint32_t>()->default_value(4), "Number of bench run")
  ("verbose,v", boost::program_options::value<uint32_t>()->default_value(1), "print all intermediate results")
  ("isActive,a",boost::program_options::value<uint32_t>()->default_value(1), "1 if the node will send data locally, otherwise just receive data from remote")
  ("syncOn,s",boost::program_options::value<uint32_t>()->default_value(1), "1 if turn on node sync, otherwise no sync between nodes");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);
  
  uint32_t n_page = commandLineArgs["nPage"].as<uint32_t>();
  double dup_ratio = commandLineArgs["dupRatio"].as<double>();
  double hash_table_fullness = commandLineArgs["fullness"].as<double>();
  uint32_t throu_factor = commandLineArgs["throuFactor"].as<uint32_t>();
  uint32_t n_bench_run = commandLineArgs["nBenchRun"].as<uint32_t>();
  uint32_t write_op_num = commandLineArgs["writeOpNum"].as<uint32_t>();
  bool verbose = static_cast<bool>(commandLineArgs["verbose"].as<uint32_t>());
  bool is_active = static_cast<bool>(commandLineArgs["isActive"].as<uint32_t>());
  bool sync_on = static_cast<bool>(commandLineArgs["syncOn"].as<uint32_t>());

  /************************************************/
  /************************************************/
  /* Module 1: get config */
  /************************************************/
  /************************************************/
  string project_dir = std::filesystem::current_path();
  auto dedup_sys = dedup::dedupSys();
  dedup_sys.initializeNetworkInfo(project_dir);
  dedup_sys.setupConnections();
  sync_on && dedup_sys.syncBarrier();
  sleep(1);
  dedup_sys.exchangeQps();
  sleep(1);
  sync_on && dedup_sys.syncBarrier();
  /************************************************/
  /************************************************/
  /* Module 2: run dedup */
  /************************************************/
  /************************************************/
  string output_dir = project_dir + "/sw/examples/dedup_sys_test1/page_resp/" + dedup_sys.node_host_name;
  // string 
  // Create the directory with read/write/search permissions for owner and group, and with read/search permissions for others
  try {
    // Create the directory and all its parent directories if they do not exist.
    if (!std::filesystem::create_directories(output_dir)) {
      std::cout << "Directory already exists or cannot be created: " << output_dir << std::endl;
    } else {
      std::cout << "Directory created: " << output_dir << std::endl;
    }
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << e.what() << '\n';
  }

  // checkings
  assert(n_page%16 == 0 && "n_page should be an multiple of 16.");
  assert(n_page%write_op_num==0 && "n_page should be an multiple of writeOpNum.");

  // Handles and alloc
  cProcess cproc(TARGET_REGION, getpid());

  // Step 0: 
  std::cout << endl << "Step0: system init" << endl;
  dedup_sys.setSysInit(&cproc);
  // setup routing table
  dedup_sys.setRoutingTable(&cproc);
  // confirm the init is done
  dedup_sys.waitSysInitDone(&cproc);

  if (is_active) {
    // drived parameters
    uint32_t initial_page_unique_count = (((uint32_t) (hash_table_fullness * dedupSys::node_ht_size) + 15)/16) * 16;
    uint32_t new_page_unique_count = (((uint32_t) (n_page * (1 - dup_ratio)) + 15)/16) * 16;
    uint32_t existing_page_unique_count =  n_page - new_page_unique_count;
    uint32_t total_page_unique_count = initial_page_unique_count + new_page_unique_count;
    assert (initial_page_unique_count >= 0);
    assert (existing_page_unique_count <= initial_page_unique_count);
    assert (total_page_unique_count <= dedupSys::node_ht_size);
    std::cout << "Config: "<< endl;
    std::cout << "1. number of initial page to fill up: " << initial_page_unique_count << endl;
    std::cout << "2. number of page to run benchmark: " << n_page << endl;
    std::cout << "3. number of existing page in benchmark: " << existing_page_unique_count << endl;
    std::cout << "4. number of new page in benchmark: " << new_page_unique_count << endl;
    
    // Step 1: Insert all initial and new pages, get SHA3
    std::cout << endl << "Step1: get all page SHA3, total unique page count: "<< total_page_unique_count << endl;
    verbose && (std::cout << "Preparing unique page data" << endl);
    char* all_unique_page_buffer = (char*) malloc(total_page_unique_count * dedupSys::pg_size);
    uint32_t* all_unique_page_sha3 = (uint32_t*) malloc(total_page_unique_count * 32); // SHA3 256bit = 32B
    assert(all_unique_page_buffer != NULL);
    assert(all_unique_page_sha3   != NULL);
    // get unique page data
    int urand = open("/dev/urandom", O_RDONLY);
    int res = read(urand, all_unique_page_buffer, total_page_unique_count * dedupSys::pg_size);
    close(urand);

    int* goldenPgIsExec = (int*) malloc(total_page_unique_count * sizeof(int));
    int* goldenPgRefCount = (int*) malloc(total_page_unique_count * sizeof(int));
    int* goldenPgIdx = (int*) malloc(total_page_unique_count * sizeof(int));
    memset(goldenPgRefCount, 0, total_page_unique_count * sizeof(int));

    Context ctx{dedup_sys, cproc, all_unique_page_buffer, all_unique_page_sha3, verbose, sync_on, goldenPgIsExec, goldenPgRefCount, goldenPgIdx};
    // support 64x2M page address mapping, do 32 in one insertion round
    uint32_t n_insertion_round = (total_page_unique_count + 32 * dedupSys::pg_per_huge_pg - 1) / (32 * dedupSys::pg_per_huge_pg);
    std::cout << "insert all unique pages, in "<< n_insertion_round << " rounds"<< endl;

    for (int insertion_round_idx = 0; insertion_round_idx < n_insertion_round; insertion_round_idx++) {
      uint32_t pg_idx_start = insertion_round_idx * 32 * dedupSys::pg_per_huge_pg;
      uint32_t pg_idx_end = ((pg_idx_start + 32 * dedupSys::pg_per_huge_pg) > total_page_unique_count) ? total_page_unique_count : (pg_idx_start + 32 * dedupSys::pg_per_huge_pg);
      uint32_t pg_idx_count = pg_idx_end - pg_idx_start;
      stringstream outfile_name;
      outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step1.txt";
      std::cout << "round " << insertion_round_idx;

      double time;
      modPages(ctx, Instr::WRITE, 1, 100, pg_idx_start, pg_idx_count, outfile_name, time);
    }

    std::cout << endl << "Step2: clean new pages" << endl;
    if (new_page_unique_count >= 0) {
      std::stringstream outfile_name;
      outfile_name << output_dir << "/resp_"<< timeStamp.str() << "_step2.txt";

      double time;
      modPages(ctx, Instr::ERASE, new_page_unique_count, 0, initial_page_unique_count, new_page_unique_count, outfile_name, time);
    }

    std::cout << endl << "Step3: start benchmarking, insertion only" << endl;
    std::vector<double> times_lst;
    std::cout << n_bench_run << " runs in total" << endl;
    for (int bench_idx = 0; bench_idx < n_bench_run; bench_idx++) {
      // create page insertion order
      vector<uint32_t> benchmark_page_idx_lst;
      {
        vector<uint32_t> random_old_page_idx_lst;
        for (int i = 0; i < initial_page_unique_count; i++){
          random_old_page_idx_lst.push_back(i);
        }
        
        random_shuffle(random_old_page_idx_lst.begin(), random_old_page_idx_lst.end());
        
        for (int i = 0; i < n_page; i++){
          if (i < new_page_unique_count){
            benchmark_page_idx_lst.push_back(i + initial_page_unique_count);
          } else {
            // random one from old
            benchmark_page_idx_lst.push_back(random_old_page_idx_lst[i - new_page_unique_count]);
          }
        }
      }
      random_shuffle(benchmark_page_idx_lst.begin(), benchmark_page_idx_lst.end());

      std::stringstream outfile_name;
      outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step3_1.txt";
      double time;
      verbose && (std::cout << endl << "starting run " << bench_idx + 1 << "/" << n_bench_run << endl);
      modPages(ctx, Instr::WRITE, write_op_num, 100, benchmark_page_idx_lst, outfile_name, time);
      times_lst.push_back(time);

      outfile_name = std::stringstream();
      outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step3_2.txt";
      modPages(ctx, Instr::ERASE, n_page, 0, benchmark_page_idx_lst, outfile_name, time);
    }

    std::cout << endl << "benchmarking done, avg time used: " << vctr_avg(times_lst) << " ns" << endl;

    std::cout << endl << "Step4: clean up all remaining pages" << endl;
    if (initial_page_unique_count >= 0) {
      std::stringstream outfile_name;
      outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step4.txt";
      double time;
      modPages(ctx, Instr::ERASE, initial_page_unique_count, 0, 0, initial_page_unique_count, outfile_name, time);
    }

    cproc.clearCompleted();
    free(all_unique_page_buffer);
    free(all_unique_page_sha3);
    free(goldenPgIsExec);
    free(goldenPgRefCount);
    free(goldenPgIdx);
  }

  return EXIT_SUCCESS;
}