#include "dmdedup.hpp"
#include <stdint.h>
#include <stddef.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <algorithm>

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <openssl/evp.h>
#include <boost/program_options.hpp>

using namespace std;
using namespace dmdedup;

static int dm_dedup_ctr(struct dedup_config *dc, uint64_t data_size)
{
  struct init_param_inram iparam_inram;
  // struct init_param_cowbtree iparam_cowbtree;
  void *iparam = NULL;
  struct metadata *md = NULL;

  dc->pblocks = data_size;
  /* Meta-data backend specific part */
  dc->mdops = &metadata_ops_inram;
  iparam_inram.blocks = data_size;
  iparam = &iparam_inram;

  bool unformatted;
  md = dc->mdops->init_meta(iparam, &unformatted);
  if (md == NULL) {
    throw std::runtime_error("failed to initialize backend metadata");
  }

  uint32_t crypto_key_size = 32;

  dc->kvs_hash_pbn = dc->mdops->kvs_create_sparse(md, crypto_key_size,
        sizeof(struct hash_pbn_value),
        dc->pblocks, unformatted);
  if (dc->kvs_hash_pbn == NULL) {
    throw std::runtime_error("failed to initialize backend metadata");
  }
  dc->bmd = md;
  dc->crypto_key_size = crypto_key_size;
  return 0;
}


/* Dmdedup destructor. */
static void dm_dedup_dtr(struct dedup_config *dc)
{
  dc->mdops->exit_meta(dc->bmd);

  free(dc);
}

struct ht_probe {
  uint64_t element_counter;
  dedup_config* dc;
};

// Example callback function
int printKeyValue(void *key, int32_t ksize, void *value, int32_t vsize, void *data) {
    // // Create strings from key and value
    // std::string keyStr(static_cast<char*>(key), ksize);
    // std::string valueStr(static_cast<char*>(value), vsize);
    ht_probe* probe = (ht_probe*)(data);
    probe->element_counter = probe->element_counter + 1;
    // Print key and value
    cout << "Element: "  << probe->element_counter << endl;
    cout << "Hash: ";
    mem_print(key, ksize);
    uint64_t pbn = *(uint64_t*) value;
    cout << "PBN: " << pbn << endl;
    // mem_print(value, vsize);
    cout << "Ref Count: " << probe->dc->mdops->get_refcount(probe->dc->bmd, pbn) << endl;

    // The callback function usually returns 0 to continue iteration or non-zero to stop.
    return 0;
}

void write_page(struct dedup_config *dc, void * hash, uint64_t* pbn_new){
  int32_t write_result;
  // uint64_t pbn_new = 0;
  // struct hash_pbn_value hashpbn_value;
  int32_t vsize;
  int32_t lookup_result;
  struct hash_pbn_value hashpbn_value;
  lookup_result = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
                  dc->crypto_key_size,
                  &hashpbn_value, &vsize);
  if (lookup_result == -ENODATA){
    // std::cout<< "new element" << std::endl;
    /* Create a new lbn-pbn mapping for given lbn */
    write_result = dc->mdops->alloc_data_block(dc->bmd, pbn_new); // -ENOSPC
    if (write_result < 0){
      throw std::runtime_error("No space");
    }
    /* Inserts new hash-pbn mapping for given hash. */
    hashpbn_value.pbn = *pbn_new;
    // std::cout << "alloc pbn: " << hashpbn_value.pbn << std::endl;
    // mem_print(hash, dc->crypto_key_size);
    write_result = dc->kvs_hash_pbn->kvs_insert(dc->kvs_hash_pbn, (void *)hash,
            dc->crypto_key_size,
            (void *)&hashpbn_value,
            sizeof(hashpbn_value));
    if (write_result < 0){
      throw std::runtime_error("Insertion error: " + std::to_string(write_result));
    }
  } else if (lookup_result == 0){
    // std::cout<< "existing element" << std::endl;
  } else {
    throw std::runtime_error("Hash Lookup error");
  }
  /* Increments refcount for new pbn entry created. */
  write_result = dc->mdops->inc_refcount(dc->bmd, hashpbn_value.pbn);
  if (write_result < 0){
    throw std::runtime_error("Refcounter inc error");
  }
  int ref_count = dc->mdops->get_refcount(dc->bmd, hashpbn_value.pbn);
  // cout << "ref count: " << ref_count << ", pbn: " << hashpbn_value.pbn << endl;;
}

int allocate_block(struct dedup_config *dc, uint64_t *pbn_new)
{
  int r;

  r = dc->mdops->alloc_data_block(dc->bmd, pbn_new);

  return r;
}

static int alloc_pbnblk_and_insert_lbn_pbn(struct dedup_config *dc, uint64_t *pbn_new)
{
  int r = 0;

  r = allocate_block(dc, pbn_new);
  if (r < 0) {
    r = -EIO;
    return r;
  }

  return r;
}

static int __handle_no_lbn_pbn(struct dedup_config *dc, void* hash)
{
  int r, ret;
  uint64_t pbn_new = 0;
  struct hash_pbn_value hashpbn_value;

  /* Create a new lbn-pbn mapping for given lbn */
  r = alloc_pbnblk_and_insert_lbn_pbn(dc, &pbn_new);
  if (r < 0)
    goto out;

  /* Inserts new hash-pbn mapping for given hash. */
  hashpbn_value.pbn = pbn_new;
  r = dc->kvs_hash_pbn->kvs_insert(dc->kvs_hash_pbn, (void *)hash,
           dc->crypto_key_size,
           (void *)&hashpbn_value,
           sizeof(hashpbn_value));
  if (r < 0)
    goto kvs_insert_err;

  /* Increments refcount for new pbn entry created. */
  // r = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // if (r < 0)
  // 	goto inc_refcount_err;

  goto out;

/* Error handling code path */
inc_refcount_err:
  /* Undo actions taken in hash-pbn kvs insert. */
  ret = dc->kvs_hash_pbn->kvs_delete(dc->kvs_hash_pbn,
             (void *)hash, dc->crypto_key_size);
  if (ret < 0){
    // DMERR("Error in deleting previously created hash pbn entry.");
    throw runtime_error("Error in deleting previously created hash pbn entry.");
  }
kvs_insert_err:
  ret = dc->mdops->dec_refcount(dc->bmd, pbn_new);
  if (ret < 0){
    // DMERR("ERROR in decrementing previously incremented refcount.");
    throw runtime_error("ERROR in decrementing previously incremented refcount.");
  }
out:
  return r;
}

static int handle_write_no_hash(struct dedup_config *dc, void* hash)
{
  int r;
  uint32_t vsize;
  r = __handle_no_lbn_pbn(dc, hash);
  return r;
}

static int __handle_no_lbn_pbn_with_hash(struct dedup_config *dc,
           void * hash,
           uint64_t pbn_this)
{
  int r = 0, ret;

  /* Increments refcount of this passed pbn */
  r = dc->mdops->inc_refcount(dc->bmd, pbn_this);
  if (r < 0)
    goto out;

out:
  return r;
}

static int handle_write_with_hash(struct dedup_config *dc,  void * hash,
          struct hash_pbn_value hashpbn_value)
{
  int r;
  uint32_t vsize;
  uint64_t pbn_this;

  pbn_this = hashpbn_value.pbn;
  r = __handle_no_lbn_pbn_with_hash(dc, hash, pbn_this);
  return r;
}

static int handle_write(struct dedup_config *dc, void * hash)
{
  int32_t vsize;
  struct hash_pbn_value hashpbn_value;
  int r;

  r = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
           dc->crypto_key_size,
           &hashpbn_value, &vsize);

  if (r == -ENODATA)
    r = handle_write_no_hash(dc, hash);
  else if (r == 0)
    r = handle_write_with_hash(dc, hash, hashpbn_value);

  if (r < 0)
    return r;

  return 0;
}

static int handle_erase(struct dedup_config *dc, void * hash)
{
  int32_t vsize;
  struct hash_pbn_value hashpbn_value;
  int r;

  r = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
           dc->crypto_key_size,
           &hashpbn_value, &vsize);
  if (r < 0)
    throw std::runtime_error("no element to erase");

  /* Increments refcount for new pbn entry created. */
  int ref_count = dc->mdops->get_refcount(dc->bmd, hashpbn_value.pbn);

  if (ref_count < 0) {
    return -1;
  } else if (ref_count == 1) {
    dc->mdops->dec_refcount(dc->bmd, hashpbn_value.pbn);
    r = dc->kvs_hash_pbn->kvs_delete(dc->kvs_hash_pbn, (void *)hash,
            dc->crypto_key_size);
  } else {
    dc->mdops->dec_refcount(dc->bmd, hashpbn_value.pbn);
  }

  return 0;
}

static int handle_read(struct dedup_config *dc, void * hash)
{
  int32_t vsize;
  struct hash_pbn_value hashpbn_value;
  int r;

  r = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
           dc->crypto_key_size,
           &hashpbn_value, &vsize);
  if (r < 0)
    throw std::runtime_error("no element to read");

  return 0;
}

static void process_bio(struct dedup_config *dc, int32_t op, void * hash, int32_t * status, uint64_t *counter = nullptr)
{
  int r;
  switch (op) {
  case 1:
    r = handle_write(dc, hash);
    break;
  case 2:
    r = handle_erase(dc, hash);
    break;
  case 3:
    r = handle_read(dc, hash);
  }

  *status = r;
  (counter != nullptr) && (*counter = *counter + 1);
}

void test(){ 
  struct dedup_config *dc;
  uint64_t data_size = 32; // number of pblocks(unique pages) to manage
  dc = (dedup_config*)malloc(sizeof(*dc));
  if(dc == nullptr){
    throw std::runtime_error("invalid malloc");
  }
  std::memset(dc, 0, sizeof(*dc)); // Initialize memory to zero
  dm_dedup_ctr(dc, data_size);
  /* initialization done */
  uint64_t hash_value1 = 1;
  uint64_t hash_value2 = 2;
  uint64_t hash_value3 = 8848;
  uint64_t* hash1 = (uint64_t*)malloc(dc->crypto_key_size);
  uint64_t* hash2 = (uint64_t*)malloc(dc->crypto_key_size);
  uint64_t* hash3 = (uint64_t*)malloc(dc->crypto_key_size);
  // std::stringstream ss1;
  // std::stringstream ss2;
  // ss1 << std::hex << std::setw(dc->crypto_key_size) << std::setfill('0') << hash_value1;
  // ss2 << std::hex << std::setw(dc->crypto_key_size) << std::setfill('0') << hash_value2;
  // memcpy(hash1, ss1.str().c_str(), dc->crypto_key_size);
  // memcpy(hash2, ss2.str().c_str(), dc->crypto_key_size);
  memset(hash1, 0, dc->crypto_key_size);
  memset(hash2, 0, dc->crypto_key_size);
  memset(hash3, 0, dc->crypto_key_size);
  hash1[0] = hash_value1;
  hash2[0] = hash_value2;
  hash3[0] = hash_value1;
  hash3[1] = hash_value3;
  // cout << hash1[0] << endl;
  // cout << hash2[0] << endl;
  // mem_print(hash1, dc->crypto_key_size);
  // mem_print(hash2, dc->crypto_key_size);

  /* step 1  insert*/
  int32_t op_status;
  uint64_t pbn_new = 0;
  int ref_count;
  // mem_print(hash1, dc->crypto_key_size);
  // mem_print(hash2, dc->crypto_key_size);
  std::cout << "start timer...... "<< std::endl;
  auto begin_time = std::chrono::high_resolution_clock::now();
  // for (int i =0; i<16; i++){
  // write_page(dc, hash1, &pbn_new);
  process_bio(dc, 1, hash1, &op_status);
  process_bio(dc, 1, hash2, &op_status);
  process_bio(dc, 1, hash1, &op_status);
  process_bio(dc, 1, hash3, &op_status);
  process_bio(dc, 2, hash1, &op_status);
  process_bio(dc, 3, hash3, &op_status);
  process_bio(dc, 2, hash2, &op_status);
  process_bio(dc, 2, hash1, &op_status);
  process_bio(dc, 2, hash3, &op_status);

  // write_page(dc, hash2, &pbn_new);
  // write_page(dc, hash1, &pbn_new);
  // }
  auto end_time = std::chrono::high_resolution_clock::now();
  double time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
  std::cout << "time used: " << time << "ns" << std::endl;
  // ref_count = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // std::cout << "pbn: " << pbn_new << ", refcount: " << ref_count << std::endl;

  // write_page(dc, hash1, &pbn_new);
  // ref_count = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // std::cout << "pbn: " << pbn_new << ", refcount" << ref_count << std::endl;

  // write_page(dc, hash2, &pbn_new);
  // ref_count = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // std::cout << "pbn: " << pbn_new << ", refcount" << ref_count << std::endl;

  // /* step 2  lookup*/
  // // struct hash_pbn_value hashpbn_value;
  // int32_t vsize;
  // int32_t lookup_result;
  // struct hash_pbn_value hashpbn_value;
  // lookup_result = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash1,
  // 								dc->crypto_key_size,
  // 								&hashpbn_value, &vsize);
  // if (lookup_result == -ENODATA){
  // 	std::cout<< "new element" << std::endl;
  // } else if (lookup_result == 0){
  // 	std::cout<< "existing element" << std::endl;
  // } else {
  // 	throw std::runtime_error("Hash Lookup error");
  // }
  /* step 3 print */
  


  // lookup_result = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
  // 												dc->crypto_key_size,
  // 												&hashpbn_value, &vsize);
  ht_probe probe;
  probe.element_counter = 0;
  probe.dc = dc;
  dc->kvs_hash_pbn->kvs_iterate(dc->kvs_hash_pbn, printKeyValue, &probe);


  /* destructor */
  dm_dedup_dtr(dc);
  return;
}


///////////////////////////////////
//
// CPU BASELINE
//
///////////////////////////////////
constexpr size_t pg_size = 4 * 1024;  // 4KiB pages
constexpr size_t hash_size = 32; // 256-bit (32-byte) hash size
std::mutex queue_mutex;
std::condition_variable queue_cv;
std::queue<std::vector<unsigned char>> hash_queue;
bool done = false;
int active_workers = 0;

// SHA2-256
// void hash_page(char* page) {
//   unsigned char hash[hash_size];
//   SHA256_CTX sha256;
//   SHA256_Init(&sha256);
//   SHA256_Update(&sha256, page, pg_size);
//   SHA256_Final(hash, &sha256);

//   std::vector<unsigned char> hash_vec(hash, hash + hash_size);
//   {
//     std::lock_guard<std::mutex> lock(queue_mutex);
//     hash_queue.push(hash_vec);
//   }
//   queue_cv.notify_one();
// }

// SHA3-256
void hash_page(char* page) {
  unsigned char hash[hash_size];
  EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
  if (mdctx == nullptr) {
    std::cerr << "Failed to create SHA3-256 context." << std::endl;
    return;
  }

  if (EVP_DigestInit_ex(mdctx, EVP_sha3_256(), nullptr) != 1) {
    std::cerr << "Failed to initialize SHA3-256 context." << std::endl;
    EVP_MD_CTX_free(mdctx);
    return;
  }

  if (EVP_DigestUpdate(mdctx, page, pg_size) != 1) {
    std::cerr << "Failed to update SHA3-256 hash." << std::endl;
    EVP_MD_CTX_free(mdctx);
    return;
  }

  if (EVP_DigestFinal_ex(mdctx, hash, nullptr) != 1) {
    std::cerr << "Failed to finalize SHA3-256 hash." << std::endl;
    EVP_MD_CTX_free(mdctx);
    return;
  }

  EVP_MD_CTX_free(mdctx);

  std::vector<unsigned char> hash_vec(hash, hash + hash_size);
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    hash_queue.push(hash_vec);
  }
  queue_cv.notify_one();
}

void worker(char* page_buffer, size_t total_page_count, size_t num_threads, size_t worker_id) {
  size_t pages_per_worker = (total_page_count + num_threads - 1) / num_threads; // Ceiling division
  size_t start_page = worker_id * pages_per_worker;
  size_t end_page = std::min(start_page + pages_per_worker, total_page_count);

  for (size_t i = start_page; i < end_page; ++i) {
    hash_page(page_buffer + i * pg_size);
  }
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    active_workers--;

    if (active_workers == 0) {
      done = true;
    }
  }
  queue_cv.notify_one();
}

void process_hashes(struct dedup_config* dc, int32_t op, int32_t* status, uint64_t *counter) {
  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    queue_cv.wait(lock, [] { return !hash_queue.empty() || done; });
    while (!hash_queue.empty()) {
      std::vector<unsigned char> hash = hash_queue.front();
      hash_queue.pop();
      lock.unlock();
      // process hash: static void process_bio(struct dedup_config *dc, int32_t op, void * hash, int32_t * status)
      process_bio(dc, op, hash.data(), status, counter);

      lock.lock();
    }

    if (done && hash_queue.empty()) break;
  }
}


void hash_table_process(char* page_buffer, size_t total_page_count, size_t num_threads, struct dedup_config* dc, int32_t op, int32_t* status, uint64_t *counter) {
  std::vector<std::thread> hash_compute_threads;
  active_workers = num_threads;
  done = false;

  for (size_t i = 0; i < num_threads; ++i) {
    hash_compute_threads.emplace_back(worker, page_buffer, total_page_count, num_threads, i);
  }

  std::thread hash_table_thread(process_hashes, dc, op, status, counter);

  for (auto& thread : hash_compute_threads) {
    thread.join();
  }
  hash_table_thread.join();
}

int main(int argc, char *argv[])
{ 
  // ---------------------------------------------------------------
  // Args 
  // ---------------------------------------------------------------
  boost::program_options::options_description programDescription("Options:");
  programDescription.add_options()
  ("nPage,n", boost::program_options::value<uint64_t>()->default_value(16384), "Number of Pages in 1 Batch")
  ("dupRatio,d", boost::program_options::value<double>()->default_value(0.5), "Overlap(batch, existing page)/nPage")
  ("fullness,f", boost::program_options::value<double>()->default_value(0.5), "fullness of hash table")
  ("nBenchRun,r", boost::program_options::value<uint64_t>()->default_value(4), "Number of bench run")
  ("numHashThreads,h", boost::program_options::value<uint64_t>()->default_value(32), "Number of thread for hash computation");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);
  
  uint64_t n_page = commandLineArgs["nPage"].as<uint64_t>();
  double dup_ratio = commandLineArgs["dupRatio"].as<double>();
  double hash_table_fullness = commandLineArgs["fullness"].as<double>();
  uint64_t n_bench_run = commandLineArgs["nBenchRun"].as<uint64_t>();
  size_t num_threads = commandLineArgs["numHashThreads"].as<size_t>();

  struct dedup_config *dc;
  // uint64_t data_size = 32768 * 8 * 1024 * 4; // number of pblocks(unique pages) to manage
  // uint64_t data_size = 32768 * 8 * 256; // number of pblocks(unique pages) to manage
  // uint64_t data_size = 32768 * 8 * 64; // number of pblocks(unique pages) to manage
  uint64_t data_size = 32768 * 8; // number of pblocks(unique pages) to manage
  dc = (dedup_config*)malloc(sizeof(*dc));
  if(dc == nullptr){
    throw std::runtime_error("invalid malloc");
  }
  std::memset(dc, 0, sizeof(*dc)); // Initialize memory to zero
  dm_dedup_ctr(dc, data_size);
  /* initialization done */
  // uint64_t n_page            = 16384;//16384;
  // double dup_ratio           = 0;
  // double hash_table_fullness = 0.5; //0.9296875;
  // uint64_t n_bench_run       = 16;
  // size_t num_threads         = 8;

  // static const uint64_t pg_size = 4096;
  uint64_t total_old_page_unique_count = (uint64_t) (hash_table_fullness * data_size);
  uint64_t batch_new_page_unique_count = (uint64_t) (n_page * (1 - dup_ratio));
  uint64_t batch_old_page_unique_count =  n_page - batch_new_page_unique_count;
  uint64_t total_page_unique_count = total_old_page_unique_count + batch_new_page_unique_count;
  char* all_unique_page_buffer = (char*) malloc(total_page_unique_count * pg_size); // 4kiB Pages
  if(all_unique_page_buffer == nullptr){
    throw std::runtime_error("invalid malloc: all_unique_page_buffer");
  }
    
  // uint64_t* all_unique_page_sha3 = (uint64_t*) malloc(total_page_unique_count * 32); // SHA3 256bit = 32B

  // Gen all unique Pages
  int urand = open("/dev/urandom", O_RDONLY);
  int res = read(urand, all_unique_page_buffer, total_page_unique_count * pg_size);
  close(urand);

  // get random pages' hash
  // int urand = open("/dev/urandom", O_RDONLY);
  // int res = read(urand, all_unique_page_sha3, total_page_unique_count * 32);
  // close(urand);

  /* step 1  insert*/
  int32_t op_status;
  uint64_t step1_counter = 0;
  std::cout << "initializing hash table...... "<< std::endl;
  auto begin_time = std::chrono::high_resolution_clock::now();
  // hash_table_process(char* page_buffer, size_t total_page_count, size_t num_threads, struct dedup_config* dc, int32_t op, int32_t* status)
  hash_table_process(all_unique_page_buffer, total_old_page_unique_count, num_threads, dc, 1, &op_status, &step1_counter);
  // for (int i = 0; i < total_old_page_unique_count; i++){
  //   process_bio(dc, 1, all_unique_page_sha3 + i * 8, &op_status);
  // }
  auto end_time = std::chrono::high_resolution_clock::now();
  double time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
  std::cout << "time used: " << time << "ns" << std::endl;
  std::cout << "page processed: " << step1_counter << " / " << total_old_page_unique_count << std::endl;

  /* step 2 benchmark*/
  uint64_t benchmark_insert_pg_num = n_page;
  vector<int> benchmark_insert_page_idx_lst;
  {
    vector<int> random_old_page_idx_lst;
    for (int i = 0; i < total_old_page_unique_count; i++){
      random_old_page_idx_lst.push_back(i);
    }
    
    random_shuffle(random_old_page_idx_lst.begin(), random_old_page_idx_lst.end());
    
    for (int i = 0; i < benchmark_insert_pg_num; i++){
      if (i < batch_new_page_unique_count){
        benchmark_insert_page_idx_lst.push_back(i + total_old_page_unique_count);
      } else {
        // random one from old
        benchmark_insert_page_idx_lst.push_back(random_old_page_idx_lst[i - batch_new_page_unique_count]);
      }
    }
  }
  random_shuffle(benchmark_insert_page_idx_lst.begin(), benchmark_insert_page_idx_lst.end());
  // create new page buffer
  char* benchmark_page_buffer = (char*)malloc(benchmark_insert_pg_num * pg_size);
  if(benchmark_page_buffer == nullptr){
    throw std::runtime_error("invalid malloc: benchmark_page_buffer");
  }
  // Copy pages from the original buffer to the new buffer based on the index list
  for (uint64_t i = 0; i < benchmark_insert_pg_num; ++i) {
    uint64_t page_idx = benchmark_insert_page_idx_lst[i];
    // std::cout << page_idx << ", "<< i << std::endl;
    std::memcpy(benchmark_page_buffer + i * pg_size, all_unique_page_buffer + page_idx * pg_size, pg_size);
  }
  uint64_t step2_counter = 0;
  std::cout << "benchmarking...... "<< std::endl;
  auto bench_begin_time = std::chrono::high_resolution_clock::now();
  // hash_table_process(benchmark_page_buffer, benchmark_insert_pg_num, num_threads, dc, 1, &op_status, nullptr);
  hash_table_process(benchmark_page_buffer, benchmark_insert_pg_num, num_threads, dc, 1, &op_status, &step2_counter);
  // for (int i = 0; i < benchmark_insert_pg_num; i ++){
  //   int randPgIdx = benchmark_insert_page_idx_lst[i];
  //   process_bio(dc, 1, all_unique_page_sha3 + randPgIdx * 8, &op_status);
  //   // memcpy(initPtrChar + pgIdx * pg_size, uniquePageBuffer + pgIdx * pg_size, pg_size);
  // }

  // for (int i = 0; i < total_old_page_unique_count; i++){
  //   process_bio(dc, 1, all_unique_page_sha3 + i * 8, &op_status);
  // }
  auto bench_end_time = std::chrono::high_resolution_clock::now();
  double bench_time = std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end_time - bench_begin_time).count();
  std::cout << "time used: " << bench_time << "ns" << std::endl;
  std::cout << "page processed: " << step2_counter << " / " << benchmark_insert_pg_num << std::endl;

  /* step 1  insert*/
  // int32_t op_status;
  // uint64_t pbn_new = 0;
  // int ref_count;
  // // mem_print(hash1, dc->crypto_key_size);
  // // mem_print(hash2, dc->crypto_key_size);
  // std::cout << "start timer...... "<< std::endl;
  // auto begin_time = std::chrono::high_resolution_clock::now();
  // // for (int i =0; i<16; i++){
  // // write_page(dc, hash1, &pbn_new);
  // process_bio(dc, 1, hash1, &op_status);
  // process_bio(dc, 1, hash2, &op_status);
  // process_bio(dc, 1, hash1, &op_status);
  // process_bio(dc, 1, hash3, &op_status);
  // process_bio(dc, 2, hash1, &op_status);
  // process_bio(dc, 3, hash3, &op_status);
  // process_bio(dc, 2, hash2, &op_status);
  // process_bio(dc, 2, hash1, &op_status);
  // process_bio(dc, 2, hash3, &op_status);

  // // write_page(dc, hash2, &pbn_new);
  // // write_page(dc, hash1, &pbn_new);
  // // }
  // auto end_time = std::chrono::high_resolution_clock::now();
  // double time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();
  // std::cout << "time used: " << time << "ns" << std::endl;
  // ref_count = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // std::cout << "pbn: " << pbn_new << ", refcount: " << ref_count << std::endl;

  // write_page(dc, hash1, &pbn_new);
  // ref_count = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // std::cout << "pbn: " << pbn_new << ", refcount" << ref_count << std::endl;

  // write_page(dc, hash2, &pbn_new);
  // ref_count = dc->mdops->inc_refcount(dc->bmd, pbn_new);
  // std::cout << "pbn: " << pbn_new << ", refcount" << ref_count << std::endl;

  // /* step 2  lookup*/
  // // struct hash_pbn_value hashpbn_value;
  // int32_t vsize;
  // int32_t lookup_result;
  // struct hash_pbn_value hashpbn_value;
  // lookup_result = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash1,
  // 								dc->crypto_key_size,
  // 								&hashpbn_value, &vsize);
  // if (lookup_result == -ENODATA){
  // 	std::cout<< "new element" << std::endl;
  // } else if (lookup_result == 0){
  // 	std::cout<< "existing element" << std::endl;
  // } else {
  // 	throw std::runtime_error("Hash Lookup error");
  // }
  /* step 3 print */
  


  // lookup_result = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
  // 												dc->crypto_key_size,
  // 												&hashpbn_value, &vsize);
  // ht_probe probe;
  // probe.element_counter = 0;
  // probe.dc = dc;
  // dc->kvs_hash_pbn->kvs_iterate(dc->kvs_hash_pbn, printKeyValue, &probe);


  /* destructor */
  dm_dedup_dtr(dc);
  return 0;
}