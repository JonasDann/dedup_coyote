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
#include "benchmarkUtil.hpp"

using namespace std;
using namespace fpga;
using namespace dedup;

void loadTrace(string trace, vector<Instr> &instrs, uint32_t &ne_read_pages, uint32_t total_page_unique_count) {
  std::ifstream f(trace);
  if (f.fail()) {
    cout << "Error opening file " << trace << endl;
    return;
  }

  unordered_map<string, uint32_t> md5_map; // Assigns MD5 hashes to unique page indices
  unordered_set<uint32_t> lba_set; // Keeps track of the used LBAs to check if there is a write to an existing LBA
  uint32_t pos_start = 0;
  uint32_t pos_end, col, i = 0;
  uint32_t curr_idx = 0;
  uint32_t ne_read_idx = 0;
  string prev_line, line, token;
  prev_line = "";
  while (getline(f, line)) {
    if (strcmp(prev_line.c_str(), line.c_str()) == 0) {
      cout << "Duplicate at line " << i << " which will be ignored" << endl;
      continue;
    }
    Instr instr;
    col = 0;
    pos_start = 0;
    bool done = false;
    while (!done) {
      if (col < 8) {
        pos_end = line.find(" ", pos_start);
      } else {
        pos_end = line.size();
        done = true;
      }
      token = line.substr(pos_start, pos_end - pos_start);
      if (col == 3) {
        instr.lba = stoul(token);
      } else if (col == 5) {
        instr.opcode = (token == "W") ? WRITE : READ;
      } else if (col == 8) {
        if (auto idx = md5_map.find(token); idx != md5_map.end()) { // If the page has been seen before
          instr.pg_idx_lst.emplace_back(idx->second);
        } else { // If the page has never been seen before
          if (instr.opcode == READ) { // Handle reads to non-existing pages by inserting negative pages that are shifted later
            ne_read_idx++;
            instr.pg_idx_lst.emplace_back(-ne_read_idx);
            md5_map[token] = -ne_read_idx;
            // cout << i << " Non-existent read " << ne_read_idx << " " << token << endl;
          } else {
            instr.pg_idx_lst.emplace_back(curr_idx);
            md5_map[token] = curr_idx;
            curr_idx++;
          }
        }
      }
      pos_start = pos_end + 1;
      col++;
    }
    if (instr.opcode == WRITE) {
      if (auto lba_idx = lba_set.find(instr.lba); lba_idx != lba_set.end()) { // If this is a write to an existing LBA
        Instr erase_instr{ERASE, instr.lba, {instr.pg_idx_lst[0]}}; // We have to erase the old contents first
        instrs.emplace_back(erase_instr);
        // cout << i << " Erase old content " << instr.lba << " " << instr.opcode << " " << instr.pg_idx_lst[0] << endl;
      }
    }
    if (instr.opcode == WRITE && instrs.size() > 0 && instrs.back().opcode == WRITE && instrs.back().lba + (instrs.back().pg_idx_lst.size()) * 8 == instr.lba) { // If this is a sequential write in the context of the previous instruction
      instrs.back().pg_idx_lst.emplace_back(instr.pg_idx_lst[0]); // Add the current instructions page to the previous write
      // cout << i << " Add to previous " << instr.pg_idx_lst[0] << endl;
    } else {
      instrs.emplace_back(instr);
      // cout << i << " Emplace back " << instr.lba << " " << instr.opcode << " " << instr.pg_idx_lst[0] << endl;
    }
    if (curr_idx + ne_read_idx >= total_page_unique_count) {
      cout << "Read " << i << " lines of trace. Can't go on because we reached maximum number of unique pages (" << total_page_unique_count << ")" << endl;
      break;
    }
    lba_set.insert(instr.lba);
    prev_line = line;
    i++;
  }
  f.close();

  for (auto &instr : instrs) { // Shift all pages by number of non-existent reads
    for (auto &pg_idx : instr.pg_idx_lst) {
      pg_idx += ne_read_idx;
    }
  }
  ne_read_pages = ne_read_idx;
}

/**
 * @brief Throughput and latency tests, read and write
 * 
 */
int main(int argc, char *argv[])
{
  stringstream timeStamp = buildTimeStamp();

  // ---------------------------------------------------------------
  // Args 
  // ---------------------------------------------------------------
  boost::program_options::options_description programDescription("Options:");
  programDescription.add_options()
  ("nPage,n", boost::program_options::value<uint32_t>()->default_value(1024), "Number of Pages in 1 Batch")
  ("fullness,f", boost::program_options::value<double>()->default_value(0.5), "fullness of hash table")
  ("verbose,v", boost::program_options::value<uint32_t>()->default_value(1), "print all intermediate results")
  ("isActive,a",boost::program_options::value<uint32_t>()->default_value(1), "1 if the node will send data locally, otherwise just receive data from remote")
  ("syncOn,s",boost::program_options::value<uint32_t>()->default_value(1), "1 if turn on node sync, otherwise no sync between nodes")
  ("trace,t", boost::program_options::value<string>()->default_value(""), "Real world trace to use for benchmark");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);
  
  uint32_t n_page = commandLineArgs["nPage"].as<uint32_t>();
  double hash_table_fullness = commandLineArgs["fullness"].as<double>();
  bool verbose = static_cast<bool>(commandLineArgs["verbose"].as<uint32_t>());
  bool is_active = static_cast<bool>(commandLineArgs["isActive"].as<uint32_t>());
  bool sync_on = static_cast<bool>(commandLineArgs["syncOn"].as<uint32_t>());
  string trace = static_cast<string>(commandLineArgs["trace"].as<string>());

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
  string output_dir = project_dir + "/sw/examples/dedup_sys_trace/page_resp/" + dedup_sys.node_host_name;
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
  assert(n_page % 16 == 0 && "n_page should be an multiple of 16.");

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
    uint32_t total_page_unique_count = (((uint32_t) (hash_table_fullness * dedupSys::node_ht_size) + 15)/16) * 16;
    uint32_t ne_read_pages;
    vector<Instr> trace_instrs;
    loadTrace(trace, trace_instrs, ne_read_pages, total_page_unique_count);
    ne_read_pages = ((ne_read_pages + 15)/16) * 16; // Apparently has to be multiple of 16. No idea why
    
    std::cout << "Config: "<< endl;
    std::cout << "1. number of non-existent read pages to fill up: " << ne_read_pages << endl;
    std::cout << "2. number of page to run benchmark: " << n_page << endl;
    std::cout << "4. number of pages in benchmark: " << total_page_unique_count << endl;
    
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

    Context ctx{dedup_sys, cproc, all_unique_page_buffer, all_unique_page_sha3, verbose, sync_on, goldenPgIsExec, goldenPgRefCount, goldenPgIdx, output_dir, timeStamp};

    initPages(ctx, ne_read_pages, total_page_unique_count - ne_read_pages);

    // Step 3: Run benchmark
    std::cout << endl << "Step3: start benchmarking" << endl;
    std::vector<double> times_lst;
    size_t total_page_count = 0;
    uint32_t start = 0, end = 0, page_count, i = 0;
    while (end < trace_instrs.size()) {
      page_count = 0;
      while (end < trace_instrs.size() && page_count + trace_instrs[end].pg_idx_lst.size() < n_page) {
        page_count += trace_instrs[end].pg_idx_lst.size();
        end++;
      }
      vector<Instr> instrs(trace_instrs.begin() + start, trace_instrs.begin() + end);
      for (auto i = 0; i < page_count % 16; i++) {
        instrs.push_back({READ, 0, {0}}); // Again need some padding for some reason
        page_count++;
      }
      std::cout << endl << "Starting run " << i << " [" << start << ",  " <<  end << "]" << endl;
      std::cout << "Number of instructions: " << end - start << endl;
      std::cout << "Number of pages: " << page_count << endl;
      std::stringstream outfile_name;
      outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step3_1.txt";
      double time;
      modPages(ctx, instrs.begin(), instrs.end(), outfile_name, time); // TODO Validation does not work with traces because parse_response assumes that every page is only touched once
      times_lst.push_back(time);
      total_page_count += page_count;
      start = end;
      i++;
    }

    auto total_time = std::accumulate(times_lst.begin(), times_lst.end(), (double) 0);
    std::cout << endl << "benchmarking done, avg time used: " << vctr_avg(times_lst) << " ns" << endl;
    cout << "total time used for " << trace_instrs.size() << " instructions: " << total_time << " ns" << endl;
    cout << "kIOPS: " << ((double) total_page_count) / (total_time / 1000000) << endl;
    cout << "GB/s: " << ((double) total_page_count * 4096) / total_time << endl;

    // TODO Cleanupo
    //std::cout << endl << "Step4: clean up all remaining pages" << endl;
    //if (initial_page_unique_count >= 0) {
    //  std::stringstream outfile_name;
    //  outfile_name << output_dir << "/resp_" << timeStamp.str() << "_step4.txt";
    //  double time;
    //  modPages(ctx, OpCode::ERASE, initial_page_unique_count, 0, 0, initial_page_unique_count, outfile_name, time);
    //}

    cproc.clearCompleted();
    free(all_unique_page_buffer);
    free(all_unique_page_sha3);
    free(goldenPgIsExec);
    free(goldenPgRefCount);
    free(goldenPgIdx);
  }

  return EXIT_SUCCESS;
}