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

// WARNING: I had problems with segmentation faults sometimes. No idea where they came from...

/**
 * @brief Fetch all sha3 hashes for a given file of pages
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
  ("nPage,n", boost::program_options::value<size_t>()->default_value(260096 * 10), "Number of pages in file")
  ("verbose,v", boost::program_options::value<uint32_t>()->default_value(1), "print all intermediate results")
  ("pages,p", boost::program_options::value<string>()->default_value(""), "Page file name");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);
  
  size_t total_page_unique_count = commandLineArgs["nPage"].as<size_t>();
  bool verbose = static_cast<bool>(commandLineArgs["verbose"].as<uint32_t>());
  string page_filename = static_cast<string>(commandLineArgs["pages"].as<string>());

  /************************************************/
  /************************************************/
  /* Module 1: get config */
  /************************************************/
  /************************************************/
  string project_dir = std::filesystem::current_path();
  auto dedup_sys = dedup::dedupSys();
  dedup_sys.initializeNetworkInfo(project_dir);
  dedup_sys.setupConnections();
  dedup_sys.syncBarrier();
  sleep(1);
  dedup_sys.exchangeQps();
  sleep(1);
  dedup_sys.syncBarrier();

  /************************************************/
  /************************************************/
  /* Module 2: fetch sha3 hashes */
  /************************************************/
  /************************************************/
  string output_dir = project_dir + "/sw/examples/fetch_sha3/page_resp/" + dedup_sys.node_host_name;
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

  // Handles and alloc
  cProcess cproc(TARGET_REGION, getpid());

  // Step 0: 
  std::cout << endl << "Step0: system init" << endl;
  dedup_sys.setSysInit(&cproc);
  // setup routing table
  dedup_sys.setRoutingTable(&cproc);
  // confirm the init is done
  dedup_sys.waitSysInitDone(&cproc);
    
  std::cout << "Config: "<< endl;
  std::cout << "1. number of pages in benchmark: " << total_page_unique_count << endl;
  
  // Step 1: Insert all initial and new pages, get SHA3
  std::cout << endl << "Step1: get all page SHA3, total unique page count: "<< total_page_unique_count << endl;
  verbose && (std::cout << "Preparing unique page data" << endl);
  char* all_unique_page_buffer = (char*) malloc(total_page_unique_count * dedupSys::pg_size);
  uint32_t* all_unique_page_sha3 = (uint32_t*) malloc(total_page_unique_count * 32); // SHA3 256bit = 32B
  assert(all_unique_page_buffer != NULL);
  assert(all_unique_page_sha3   != NULL);
  // get unique page data

  int fd;
  if ((fd = open(page_filename.c_str(), O_RDONLY)) < 0) {
      cout << "Not possible to open input page file" << endl;
  }
  size_t total_bytes_to_read = total_page_unique_count * dedupSys::pg_size;
  ssize_t status = 0;
  off_t bytes_read = 0;
  while (bytes_read < total_bytes_to_read && (status = read(fd, all_unique_page_buffer + bytes_read, total_bytes_to_read - bytes_read)) > 0) {
    bytes_read += status;
  }
  close(fd);
  if (status < 0 || bytes_read < total_bytes_to_read) {
    cout << "Error reading input page file: " << status << endl;
  }

  int* goldenPgIsExec = (int*) malloc(total_page_unique_count * sizeof(int));
  int* goldenPgRefCount = (int*) malloc(total_page_unique_count * sizeof(int));
  int* goldenPgIdx = (int*) malloc(total_page_unique_count * sizeof(int));
  memset(goldenPgRefCount, 0, total_page_unique_count * sizeof(int));

  Context ctx{dedup_sys, cproc, all_unique_page_buffer, all_unique_page_sha3, verbose, true, goldenPgIsExec, goldenPgRefCount, goldenPgIdx, output_dir, timeStamp};

  initSHA3(ctx, total_page_unique_count);

  string output_filename(page_filename);
  auto pos = output_filename.find("pages");
  output_filename.replace(pos, 5, "hashes");
  cout << "Write to " << output_filename << endl;
  if ((fd = open(output_filename.c_str(), O_WRONLY | O_CREAT, 0644)) < 0) {
    cout << "Not possible to open output file for hashes" << endl;
  }
  size_t to_write = total_page_unique_count * 32;
  size_t written = 0;
  while (written < to_write) {
    written += write(fd, ((uint8_t *) all_unique_page_sha3) + written, to_write - written);
  }
  close(fd);

  cproc.clearCompleted();
  free(all_unique_page_buffer);
  free(all_unique_page_sha3);
  free(goldenPgIsExec);
  free(goldenPgRefCount);
  free(goldenPgIdx);

  return EXIT_SUCCESS;
}