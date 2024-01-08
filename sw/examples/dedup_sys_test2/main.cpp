#include <iostream>
#include <string>
#include <malloc.h>
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
constexpr auto const targetRegion = 0;
constexpr auto const nBenchRuns = 1;  

/**
 * @brief Average it out
 * 
 */
double vctr_avg(std::vector<double> const& v) {
  return 1.0 * std::accumulate(v.begin(), v.end(), 0LL) / v.size();
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
  ("gcRatio,g", boost::program_options::value<double>()->default_value(1), "gcCount/nPage")
  ("fullness,f", boost::program_options::value<double>()->default_value(0.5), "fullness of hash table")
  ("throuFactor,t", boost::program_options::value<uint32_t>()->default_value(8), "Store throughput factor")
  ("nBenchRun,r", boost::program_options::value<uint32_t>()->default_value(4), "Number of bench run")
  ("verbose,v", boost::program_options::value<uint32_t>()->default_value(1), "print all intermediate results")
  ("isActive,a",boost::program_options::value<uint32_t>()->default_value(1), "1 if the node will send data locally, otherwise just receive data from remote")
  ("syncOn,s",boost::program_options::value<uint32_t>()->default_value(1), "1 if turn on node sync, otherwise no sync between nodes");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);
  
  uint32_t n_page = commandLineArgs["nPage"].as<uint32_t>();
  double gc_ratio = commandLineArgs["gcRatio"].as<double>();
  double hash_table_fullness = commandLineArgs["fullness"].as<double>();
  uint32_t throu_factor = commandLineArgs["throuFactor"].as<uint32_t>();
  uint32_t n_bench_run = commandLineArgs["nBenchRun"].as<uint32_t>();
  bool verbose = static_cast<bool>(commandLineArgs["verbose"].as<uint32_t>());
  bool is_active = static_cast<bool>(commandLineArgs["isActive"].as<uint32_t>());
  bool sync_on = static_cast<bool>(commandLineArgs["syncOn"].as<uint32_t>());

  /************************************************/
  /************************************************/
  /* Module 1: get config */
  /************************************************/
  /************************************************/
  string project_dir = "/home/jiayli/projects/coyote-rdma";
  auto dedup_sys = dedup::dedupSys();
  dedup_sys.initializeNetworkInfo();
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
  string output_dir = project_dir + "/sw/examples/dedup_sys_test2/page_resp/" + dedup_sys.node_host_name;
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

  // Handles and alloc
  cProcess cproc(targetRegion, getpid());

  // Step 0: 
  cout << endl << "Step0: system init" << endl;
  dedup_sys.setSysInit(&cproc);
  // setup routing table
  dedup_sys.setRoutingTable(&cproc);
  // confirm the init is done
  dedup_sys.waitSysInitDone(&cproc);

  if (is_active) {
    // drived parameters
    uint32_t total_page_unique_count = (((uint32_t) (hash_table_fullness * dedupSys::node_ht_size) + 15)/16) * 16;
    uint32_t batch_no_gc_page_unique_count = (((uint32_t) (n_page * (1 - gc_ratio)) + 15)/16) * 16;
    uint32_t batch_gc_page_unique_count =  n_page - batch_no_gc_page_unique_count;
    assert (total_page_unique_count >= 0);
    assert (batch_gc_page_unique_count >= 0);
    assert (n_page <= total_page_unique_count);
    assert (total_page_unique_count <= dedupSys::node_ht_size);
    assert (batch_gc_page_unique_count % 16 == 0);
    cout << "Config: "<< endl;
    cout << "1. number of initial page to fill up: " << total_page_unique_count << endl;
    cout << "2. number of page to run benchmark: " << n_page << endl;
    cout << "3. number of page exist after deletion: " << batch_no_gc_page_unique_count << endl;
    cout << "4. number of page GC: " << batch_gc_page_unique_count << endl;

    // Step 1: Insert all old and new pages, get SHA3
    cout << endl << "Step1: get all page SHA3, total unique page count: "<< total_page_unique_count << endl;
    verbose && (cout << "Preparing unique page data" << endl);
    char* all_unique_page_buffer = (char*) malloc(total_page_unique_count * dedupSys::pg_size);
    uint32_t* all_unique_page_sha3 = (uint32_t*) malloc(total_page_unique_count * 32); // SHA3 256bit = 32B
    assert(all_unique_page_buffer != NULL);
    assert(all_unique_page_sha3   != NULL);
    // get unique page data
    int urand = open("/dev/urandom", O_RDONLY);
    int res = read(urand, all_unique_page_buffer, total_page_unique_count * dedupSys::pg_size);
    close(urand);

    // support 64x2M page address mapping, do 32 in one insertion round
    ofstream outfile;
    if (verbose){
      std::stringstream outfile_name;
      outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step1.txt";
      outfile.open(outfile_name.str(), ios::out);
    }
    bool allPassed = true;

    uint32_t n_insertion_round = (total_page_unique_count + 32 * dedupSys::pg_per_huge_pg - 1) / (32 * dedupSys::pg_per_huge_pg);
    cout << "insert all unique pages, in "<< n_insertion_round << " rounds"<< endl;
    for (int insertion_rount_idx; insertion_rount_idx < n_insertion_round; insertion_rount_idx++){
      uint32_t pg_idx_start = insertion_rount_idx * 32 * dedupSys::pg_per_huge_pg;
      uint32_t pg_idx_end = ((pg_idx_start + 32 * dedupSys::pg_per_huge_pg) > total_page_unique_count) ? total_page_unique_count : (pg_idx_start + 32 * dedupSys::pg_per_huge_pg);
      uint32_t pg_idx_count = pg_idx_end - pg_idx_start;
      uint32_t prep_instr_pg_num = (1 * dedupSys::instr_size + dedupSys::pg_size - 1) / dedupSys::pg_size; // 1 op
      uint32_t n_hugepage_prep_req = ((pg_idx_count + prep_instr_pg_num) * dedupSys::pg_size + dedupSys::huge_pg_size -1) / dedupSys::huge_pg_size; // roundup, number of hugepage for n page
      uint32_t n_hugepage_prep_rsp = (pg_idx_count * 64 + dedupSys::huge_pg_size-1) / dedupSys::huge_pg_size; // roundup, number of huge page for 64B response from each page

      void* prepReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_prep_req});
      void* prepRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_prep_rsp});

      void* initPtr = prepReqMem;
      for (int instrIdx = 0; instrIdx < dedupSys::instr_per_page * prep_instr_pg_num; instrIdx ++){
        if (instrIdx < 1){
          // set instr: write instr
          initPtr = set_write_instr(initPtr, 100 + pg_idx_start, pg_idx_count, false);

          // set pages
          char* initPtrChar = (char*) initPtr;
          memcpy(initPtrChar, all_unique_page_buffer + pg_idx_start * dedupSys::pg_size, pg_idx_count * dedupSys::pg_size);
          initPtrChar = initPtrChar + pg_idx_count * dedupSys::pg_size;
          initPtr = (void*) initPtrChar;
        } else {
          // set Insrt: nop
          initPtr = set_nop(initPtr);
        }
      }

      // golden res
      int* prepGoldenPgIsExec = (int*) malloc(pg_idx_count * sizeof(int));
      int* prepGoldenPgRefCount = (int*) malloc(pg_idx_count * sizeof(int));
      int* prepGoldenPgIdx = (int*) malloc(pg_idx_count * sizeof(int));

      for (int pgIdx = 0; pgIdx < pg_idx_count; pgIdx ++){
        prepGoldenPgIsExec[pgIdx] = 1;
        prepGoldenPgRefCount[pgIdx] = 1;
        prepGoldenPgIdx[pgIdx] = 100 + pg_idx_start + pgIdx;
      }

      verbose && (cout << "round " << insertion_rount_idx << " start execution: page " << pg_idx_start << " to " << pg_idx_end << endl);
      dedupSys::setReqStream(&cproc, prepReqMem, pg_idx_count + prep_instr_pg_num, prepRspMem);
      sync_on && dedup_sys.syncBarrier();
      dedupSys::setStart(&cproc);
      // cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
      sleep(1);

      dedupSys::getExecDone(&cproc, pg_idx_count + prep_instr_pg_num, pg_idx_count);

      /** parse and print the page response */
      verbose && (cout << "parsing the results" << endl);
      bool check_res = parse_response(pg_idx_count, prepRspMem, prepGoldenPgIsExec, prepGoldenPgRefCount, prepGoldenPgIdx, 1, outfile);
      allPassed = allPassed && check_res;
      uint32_t* rspMemUInt32 = (uint32_t*) prepRspMem;
      verbose && (cout << "get all SHA3" << endl);
      for (int i=0; i < pg_idx_count; i++) {
        memcpy(all_unique_page_sha3 + (pg_idx_start + i) * 8, (void*) (rspMemUInt32 + i*16), 32);
      }
      free(prepGoldenPgIsExec);
      free(prepGoldenPgRefCount);
      free(prepGoldenPgIdx);
      cproc.freeMem(prepReqMem);
      cproc.freeMem(prepRspMem);
    }
    cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
    if (verbose){
      outfile.close();
    }


    cout << endl << "Step2: start benchmarking, deletion only" << endl;
    std::vector<double> times_lst;
    cout << n_bench_run << " runs in total" << endl;
    for (int bench_idx = 0; bench_idx < n_bench_run; bench_idx ++){
      verbose && (cout << endl << "starting run " << bench_idx + 1 << "/" << n_bench_run << endl);

      // get page index
      vector<int> benchmark_delete_page_idx_lst;
      vector<int> benchmark_add_ref_page_idx_lst;
      vector<int> benchmark_insert_page_idx_lst;
      {
        vector<int> random_all_page_idx_lst;
        for (int i = 0; i < total_page_unique_count; i++){
          random_all_page_idx_lst.push_back(i);
        }
        
        random_shuffle(random_all_page_idx_lst.begin(), random_all_page_idx_lst.end());
        
        for (int i = 0; i < n_page; i++){
          benchmark_delete_page_idx_lst.push_back(random_all_page_idx_lst[i]);
          if (i < batch_no_gc_page_unique_count){
            benchmark_add_ref_page_idx_lst.push_back(random_all_page_idx_lst[i]);
          } else {
            benchmark_insert_page_idx_lst.push_back(random_all_page_idx_lst[i]);
          }
        }
        random_shuffle(benchmark_delete_page_idx_lst.begin(), benchmark_delete_page_idx_lst.end());
      }

      // add reference
      if (batch_no_gc_page_unique_count > 0){
        uint32_t num_page_to_add_ref  = batch_no_gc_page_unique_count;
        uint32_t add_ref_instr_pg_num = (1 * dedupSys::instr_size + dedupSys::pg_size - 1) / dedupSys::pg_size; // data transfer is done in pages
        uint32_t n_hugepage_add_ref_req = ((num_page_to_add_ref + add_ref_instr_pg_num) * dedupSys::pg_size + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
        uint32_t n_hugepage_add_ref_rsp = (num_page_to_add_ref * 64 + dedupSys::huge_pg_size-1) / dedupSys::huge_pg_size;
        void* addRefReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_add_ref_req});
        void* addRefRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_add_ref_rsp});

        void* initPtr = addRefReqMem;
        for (int instrIdx = 0; instrIdx < add_ref_instr_pg_num * dedupSys::instr_per_page; instrIdx ++){
          if (instrIdx < 1){
            // set instr: write instr
            initPtr = set_write_instr(initPtr, 10000, num_page_to_add_ref, false);

            // set pages
            char* initPtrChar = (char*) initPtr;
            for (int pgIdx = 0; pgIdx < num_page_to_add_ref; pgIdx ++){
              int randPgIdx = benchmark_add_ref_page_idx_lst[pgIdx];
              memcpy(initPtrChar + pgIdx * dedupSys::pg_size, all_unique_page_buffer + randPgIdx * dedupSys::pg_size, dedupSys::pg_size);
              // memcpy(initPtrChar + pgIdx * pg_size, uniquePageBuffer + pgIdx * pg_size, pg_size);
            }
            initPtrChar = initPtrChar + num_page_to_add_ref * dedupSys::pg_size;
            initPtr = (void*) initPtrChar;
          } else {
            // set Insrt: nop
            initPtr = set_nop(initPtr);
          }
        }

        // golden res
        int* addRefGoldenPgIsExec = (int*) malloc(num_page_to_add_ref * sizeof(int));
        int* addRefGoldenPgRefCount = (int*) malloc(num_page_to_add_ref * sizeof(int));
        int* addRefGoldenPgIdx = (int*) malloc(num_page_to_add_ref * sizeof(int));

        for (int pgIdx = 0; pgIdx < num_page_to_add_ref; pgIdx ++){
          addRefGoldenPgIsExec[pgIdx] = 0;
          addRefGoldenPgRefCount[pgIdx] = 2;
          addRefGoldenPgIdx[pgIdx] = 10000 + pgIdx;
        }

        dedupSys::setReqStream(&cproc, addRefReqMem, num_page_to_add_ref + add_ref_instr_pg_num, addRefRspMem);
        sync_on && dedup_sys.syncBarrier();
        dedupSys::setStart(&cproc);
        // cproc.setCSR(1, static_cast<uint32_t>(CTLR::START));
        sleep(1);
        dedupSys::getExecDone(&cproc, num_page_to_add_ref + add_ref_instr_pg_num, num_page_to_add_ref);

        ofstream outfile2_1;
        if (verbose){
          std::stringstream outfile_name2_1;
          outfile_name2_1 << output_dir << "/resp_"<< timeStamp.str() << "_step2_1.txt";
          outfile2_1.open(outfile_name2_1.str(), ios::out);
        }

        verbose && (cout << "parsing the results" << endl);
        allPassed = parse_response(num_page_to_add_ref, addRefRspMem, addRefGoldenPgIsExec, addRefGoldenPgRefCount, addRefGoldenPgIdx, 1, outfile2_1);
        cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
        outfile2_1.close();
        free(addRefGoldenPgIsExec);
        free(addRefGoldenPgRefCount);
        free(addRefGoldenPgIdx);
        cproc.freeMem(addRefReqMem);
        cproc.freeMem(addRefRspMem);
      }
      
      uint32_t benchmark_num_page_to_clean = n_page;
      uint32_t benchmark_clean_instr_pg_num = (benchmark_num_page_to_clean * dedupSys::instr_size + dedupSys::pg_size - 1) / dedupSys::pg_size; // data transfer is done in pages
      uint32_t n_hugepage_bench_clean_req = (benchmark_clean_instr_pg_num * dedupSys::pg_size + dedupSys::huge_pg_size -1) / dedupSys::huge_pg_size;
      uint32_t n_hugepage_bench_clean_rsp = (benchmark_num_page_to_clean * 64 + dedupSys::huge_pg_size-1) / dedupSys::huge_pg_size;
      void* benchCleanReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_clean_req});
      void* benchCleanRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_clean_rsp});

      void* initPtr = benchCleanReqMem;
      for (int instrIdx = 0; instrIdx < benchmark_clean_instr_pg_num * dedupSys::instr_per_page; instrIdx ++){
        if (instrIdx < benchmark_num_page_to_clean){
          // set instr: erase instr
          int pgIdx = benchmark_delete_page_idx_lst[instrIdx];
          initPtr = set_erase_instr(initPtr, all_unique_page_sha3 + pgIdx * 8, false);
        } else {
          // set Insrt: nop
          initPtr = set_nop(initPtr);
        }
      }

      // golden res
      int* benchCleanGoldenPgIsExec = (int*) malloc(benchmark_num_page_to_clean * sizeof(int));
      int* benchCleanGoldenPgRefCount = (int*) malloc(benchmark_num_page_to_clean * sizeof(int));
      int* benchCleanGoldenPgIdx = (int*) malloc(benchmark_num_page_to_clean * sizeof(int));

      for (int pgIdx = 0; pgIdx < benchmark_num_page_to_clean; pgIdx ++){
        if (std::find(benchmark_add_ref_page_idx_lst.begin(), benchmark_add_ref_page_idx_lst.end(), benchmark_delete_page_idx_lst[pgIdx]) != benchmark_add_ref_page_idx_lst.end()){
          // item present: ref = 2
          benchCleanGoldenPgIsExec[pgIdx] = 0;
          benchCleanGoldenPgRefCount[pgIdx] = 1;
          benchCleanGoldenPgIdx[pgIdx] = 0;
        } else {
          // GC page
          benchCleanGoldenPgIsExec[pgIdx] = 1;
          benchCleanGoldenPgRefCount[pgIdx] = 0;
          benchCleanGoldenPgIdx[pgIdx] = 0;
        }
      }

      dedupSys::setReqStream(&cproc, benchCleanReqMem, benchmark_clean_instr_pg_num, benchCleanRspMem);
      sync_on && dedup_sys.syncBarrier();
      auto begin_time = std::chrono::high_resolution_clock::now();
      dedupSys::setStart(&cproc);
      dedupSys::waitExecDone(&cproc, benchmark_num_page_to_clean);
      auto end_time = std::chrono::high_resolution_clock::now();

      double time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
      times_lst.push_back(time);

      dedupSys::getExecDone(&cproc, benchmark_clean_instr_pg_num, benchmark_num_page_to_clean);
      if (verbose){ std::cout << "time used: " << time << "ns" << std::endl; }

      ofstream outfile2_2;
      if (verbose){
        std::stringstream outfile_name2_2;
        outfile_name2_2 << output_dir << "/resp_" << timeStamp.str() << "_step2_2.txt";
        outfile2_2.open(outfile_name2_2.str(), ios::out);
      }

      verbose && (cout << "parsing the results" << endl);
      allPassed = parse_response(benchmark_num_page_to_clean, benchCleanRspMem, benchCleanGoldenPgIsExec, benchCleanGoldenPgRefCount, benchCleanGoldenPgIdx, 2, outfile2_2);
      cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
      if (verbose){
        outfile2_2.close();
      }

      free(benchCleanGoldenPgIsExec);
      free(benchCleanGoldenPgRefCount);
      free(benchCleanGoldenPgIdx);
      cproc.freeMem(benchCleanReqMem);
      cproc.freeMem(benchCleanRspMem);

      if (batch_gc_page_unique_count > 0){
        uint32_t benchmark_insert_pg_num = batch_gc_page_unique_count;
        uint32_t benchmark_insert_instr_pg_num = (1 * dedupSys::instr_size + dedupSys::pg_size - 1) / dedupSys::pg_size; // data transfer is done in pages
        uint32_t n_hugepage_bench_insert_req = ((benchmark_insert_pg_num + benchmark_insert_instr_pg_num) * dedupSys::pg_size + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
        uint32_t n_hugepage_bench_insert_rsp = (benchmark_insert_pg_num * 64 + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
        void* benchInsertReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_insert_req});
        void* benchInsertRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_bench_insert_rsp});

        initPtr = benchInsertReqMem;
        for (int instrIdx = 0; instrIdx < dedupSys::instr_per_page * benchmark_insert_instr_pg_num; instrIdx ++){
          if (instrIdx < 1){
            // set instr: write instr
            initPtr = set_write_instr(initPtr, 100000, benchmark_insert_pg_num, false);

            // set pages
            char* initPtrChar = (char*) initPtr;
            for (int pgIdx = 0; pgIdx < benchmark_insert_pg_num; pgIdx ++){
              int randPgIdx = benchmark_insert_page_idx_lst[pgIdx];
              memcpy(initPtrChar + pgIdx * dedupSys::pg_size, all_unique_page_buffer + randPgIdx * dedupSys::pg_size, dedupSys::pg_size);
            }
            initPtrChar = initPtrChar + benchmark_insert_pg_num * dedupSys::pg_size;
            initPtr = (void*) initPtrChar;
          } else {
            // set Insrt: nop
            initPtr = set_nop(initPtr);
          }
        }

        // golden res
        int* benchInsertGoldenPgIsExec = (int*) malloc(benchmark_insert_pg_num * sizeof(int));
        int* benchInsertGoldenPgRefCount = (int*) malloc(benchmark_insert_pg_num * sizeof(int));
        int* benchInsertGoldenPgIdx = (int*) malloc(benchmark_insert_pg_num * sizeof(int));

        for (int pgIdx = 0; pgIdx < benchmark_insert_pg_num; pgIdx ++){
          benchInsertGoldenPgIsExec[pgIdx] = 1;
          benchInsertGoldenPgRefCount[pgIdx] = 1;
          benchInsertGoldenPgIdx[pgIdx] = 100000 + pgIdx;
        }

        dedupSys::setReqStream(&cproc, benchInsertReqMem, benchmark_insert_pg_num + benchmark_insert_instr_pg_num, benchInsertRspMem);
        sync_on && dedup_sys.syncBarrier();
        dedupSys::setStart(&cproc);
        sleep(1);
        dedupSys::getExecDone(&cproc, benchmark_insert_pg_num + benchmark_insert_instr_pg_num, benchmark_insert_pg_num);

        ofstream outfile2_3;
        if (verbose){
          std::stringstream outfile_name2_3;
          outfile_name2_3 << output_dir << "/resp_" << timeStamp.str() << "_step2_3.txt";
          outfile2_3.open(outfile_name2_3.str(), ios::out);
        }

        verbose && (cout << "parsing the results" << endl);
        allPassed = parse_response(benchmark_insert_pg_num, benchInsertRspMem, benchInsertGoldenPgIsExec, benchInsertGoldenPgRefCount, benchInsertGoldenPgIdx, 1, outfile2_3);
        cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
        outfile2_3.close();

        free(benchInsertGoldenPgIsExec);
        free(benchInsertGoldenPgRefCount);
        free(benchInsertGoldenPgIdx);
        cproc.freeMem(benchInsertReqMem);
        cproc.freeMem(benchInsertRspMem);
      }
    }

    cout << endl << "benchmarking done, avg time used: " << vctr_avg(times_lst) << " ns" << endl;

    cout << endl << "Step3: clean up all remaining pages" << endl;
    if (total_page_unique_count > 0){
      uint32_t final_num_page_to_clean = total_page_unique_count;
      uint32_t final_clean_instr_pg_num = (final_num_page_to_clean * dedupSys::instr_size + dedupSys::pg_size - 1) / dedupSys::pg_size; // data transfer is done in pages
      uint32_t n_hugepage_final_clean_req = (final_clean_instr_pg_num * dedupSys::pg_size + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
      uint32_t n_hugepage_final_clean_rsp = (final_num_page_to_clean * 64 + dedupSys::huge_pg_size - 1) / dedupSys::huge_pg_size;
      void* finalCleanReqMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_final_clean_req});
      void* finalCleanRspMem = cproc.getMem({CoyoteAlloc::HOST_2M, n_hugepage_final_clean_rsp});

      void* initPtr = finalCleanReqMem;
      for (int instrIdx = 0; instrIdx < final_clean_instr_pg_num * dedupSys::instr_per_page; instrIdx ++){
        if (instrIdx < final_num_page_to_clean){
          // set instr: erase instr
          int pgIdx = instrIdx;
          initPtr = set_erase_instr(initPtr, all_unique_page_sha3 + pgIdx * 8, false);
        } else {
          // set Insrt: nop
          initPtr = set_nop(initPtr);
        }
      }

      // golden res
      int* finalCleanGoldenPgIsExec = (int*) malloc(final_num_page_to_clean * sizeof(int));
      int* finalCleanGoldenPgRefCount = (int*) malloc(final_num_page_to_clean * sizeof(int));
      int* finalCleanGoldenPgIdx = (int*) malloc(final_num_page_to_clean * sizeof(int));

      for (int pgIdx = 0; pgIdx < final_num_page_to_clean; pgIdx ++){
        finalCleanGoldenPgIsExec[pgIdx] = 1;
        finalCleanGoldenPgRefCount[pgIdx] = 0;
        finalCleanGoldenPgIdx[pgIdx] = 0;
      }

      dedupSys::setReqStream(&cproc, finalCleanReqMem, final_clean_instr_pg_num, finalCleanRspMem);
      sync_on && dedup_sys.syncBarrier();
      dedupSys::setStart(&cproc);
      sleep(1);
      dedupSys::getExecDone(&cproc, final_clean_instr_pg_num, final_num_page_to_clean);

      ofstream outfile3;
      if (verbose){
        std::stringstream outfile_name3;
        outfile_name3 << output_dir << "/resp_" << timeStamp.str() << "_step3.txt";
        outfile3.open(outfile_name3.str(), ios::out);
      }

      verbose && (cout << "parsing the results" << endl);
      allPassed = parse_response(final_num_page_to_clean, finalCleanRspMem, finalCleanGoldenPgIsExec, finalCleanGoldenPgRefCount, finalCleanGoldenPgIdx, 2, outfile3);
      cout << "all page passed?: " << (allPassed ? "True" : "False") << endl;
      outfile3.close();
      free(finalCleanGoldenPgIsExec);
      free(finalCleanGoldenPgRefCount);
      free(finalCleanGoldenPgIdx);
      cproc.freeMem(finalCleanReqMem);
      cproc.freeMem(finalCleanRspMem);
    }

    cproc.clearCompleted();
    free(all_unique_page_buffer);
    free(all_unique_page_sha3);
  }

  return EXIT_SUCCESS;
}