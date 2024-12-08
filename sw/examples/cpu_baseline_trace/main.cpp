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

#include "benchmarkUtil.hpp"

using namespace std;
using namespace dmdedup;

struct DmInstr {
  OpCode op;
  std::vector<unsigned char> hash;
};

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
  if (md == NULL)
  {
    throw std::runtime_error("failed to initialize backend metadata");
  }

  uint32_t crypto_key_size = 32;

  dc->kvs_hash_pbn = dc->mdops->kvs_create_sparse(md, crypto_key_size,
                                                  sizeof(struct hash_pbn_value),
                                                  dc->pblocks, unformatted);
  if (dc->kvs_hash_pbn == NULL)
  {
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

struct ht_probe
{
  uint64_t element_counter;
  dedup_config *dc;
};

// Example callback function
int printKeyValue(void *key, int32_t ksize, void *value, int32_t vsize, void *data)
{
  // // Create strings from key and value
  // std::string keyStr(static_cast<char*>(key), ksize);
  // std::string valueStr(static_cast<char*>(value), vsize);
  ht_probe *probe = (ht_probe *)(data);
  probe->element_counter = probe->element_counter + 1;
  // Print key and value
  cout << "Element: " << probe->element_counter << endl;
  cout << "Hash: ";
  mem_print(key, ksize);
  uint64_t pbn = *(uint64_t *)value;
  cout << "PBN: " << pbn << endl;
  // mem_print(value, vsize);
  cout << "Ref Count: " << probe->dc->mdops->get_refcount(probe->dc->bmd, pbn) << endl;

  // The callback function usually returns 0 to continue iteration or non-zero to stop.
  return 0;
}

void write_page(struct dedup_config *dc, void *hash, uint64_t *pbn_new)
{
  int32_t write_result;
  // uint64_t pbn_new = 0;
  // struct hash_pbn_value hashpbn_value;
  int32_t vsize;
  int32_t lookup_result;
  struct hash_pbn_value hashpbn_value;
  lookup_result = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
                                               dc->crypto_key_size,
                                               &hashpbn_value, &vsize);
  if (lookup_result == -ENODATA)
  {
    // std::cout<< "new element" << std::endl;
    /* Create a new lbn-pbn mapping for given lbn */
    write_result = dc->mdops->alloc_data_block(dc->bmd, pbn_new); // -ENOSPC
    if (write_result < 0)
    {
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
    if (write_result < 0)
    {
      throw std::runtime_error("Insertion error: " + std::to_string(write_result));
    }
  }
  else if (lookup_result == 0)
  {
    // std::cout<< "existing element" << std::endl;
  }
  else
  {
    throw std::runtime_error("Hash Lookup error");
  }
  /* Increments refcount for new pbn entry created. */
  write_result = dc->mdops->inc_refcount(dc->bmd, hashpbn_value.pbn);
  if (write_result < 0)
  {
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
  if (r < 0)
  {
    r = -EIO;
    return r;
  }

  return r;
}

static int __handle_no_lbn_pbn(struct dedup_config *dc, void *hash)
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
  if (ret < 0)
  {
    // DMERR("Error in deleting previously created hash pbn entry.");
    throw runtime_error("Error in deleting previously created hash pbn entry.");
  }
kvs_insert_err:
  ret = dc->mdops->dec_refcount(dc->bmd, pbn_new);
  if (ret < 0)
  {
    // DMERR("ERROR in decrementing previously incremented refcount.");
    throw runtime_error("ERROR in decrementing previously incremented refcount.");
  }
out:
  return r;
}

static int handle_write_no_hash(struct dedup_config *dc, void *hash)
{
  int r;
  uint32_t vsize;
  r = __handle_no_lbn_pbn(dc, hash);
  return r;
}

static int __handle_no_lbn_pbn_with_hash(struct dedup_config *dc,
                                         void *hash,
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

static int handle_write_with_hash(struct dedup_config *dc, void *hash,
                                  struct hash_pbn_value hashpbn_value)
{
  int r;
  uint32_t vsize;
  uint64_t pbn_this;

  pbn_this = hashpbn_value.pbn;
  r = __handle_no_lbn_pbn_with_hash(dc, hash, pbn_this);
  return r;
}

static int handle_write(struct dedup_config *dc, void *hash)
{
  int32_t vsize;
  struct hash_pbn_value hashpbn_value;
  int r;

  r = dc->kvs_hash_pbn->kvs_lookup(dc->kvs_hash_pbn, hash,
                                   dc->crypto_key_size,
                                   &hashpbn_value, &vsize);

  if (r == -ENODATA) {
    r = handle_write_no_hash(dc, hash);
  } else if (r == 0) {
    r = handle_write_with_hash(dc, hash, hashpbn_value);
  }

  if (r < 0)
    return r;

  return 0;
}

static int handle_erase(struct dedup_config *dc, void *hash)
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

  if (ref_count < 0)
  {
    return -1;
  }
  else if (ref_count == 1)
  {
    dc->mdops->dec_refcount(dc->bmd, hashpbn_value.pbn);
    r = dc->kvs_hash_pbn->kvs_delete(dc->kvs_hash_pbn, (void *)hash,
                                     dc->crypto_key_size);
  }
  else
  {
    dc->mdops->dec_refcount(dc->bmd, hashpbn_value.pbn);
  }

  return 0;
}

static int handle_read(struct dedup_config *dc, void *hash)
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

static void process_bio(struct dedup_config *dc, OpCode op, void *hash, int32_t *status, uint64_t *counter = nullptr)
{
  int r;
  switch (op)
  {
  case WRITE:
    r = handle_write(dc, hash);
    break;
  case ERASE:
    r = handle_erase(dc, hash);
    break;
  case READ:
    r = handle_read(dc, hash);
  }

  *status = r;
  (counter != nullptr) && (*counter = *counter + 1);
}

///////////////////////////////////
//
// CPU BASELINE
//
///////////////////////////////////
constexpr size_t pg_size = 4 * 1024; // 4KiB pages
constexpr size_t hash_size = 32;     // 256-bit (32-byte) hash size
constexpr size_t num_threads = 64;
std::mutex queue_mutex[num_threads];
std::condition_variable queue_cv[num_threads];
std::queue<DmInstr> hash_queue[num_threads];
bool done = false;
int active_workers = 0;

// SHA3-256
void hash_page(char *page, OpCode op, size_t worker_id)
{
  unsigned char hash[hash_size];
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == nullptr)
  {
    std::cerr << "Failed to create SHA3-256 context." << std::endl;
    return;
  }

  if (EVP_DigestInit_ex(mdctx, EVP_sha3_256(), nullptr) != 1)
  {
    std::cerr << "Failed to initialize SHA3-256 context." << std::endl;
    EVP_MD_CTX_free(mdctx);
    return;
  }

  if (EVP_DigestUpdate(mdctx, page, pg_size) != 1)
  {
    std::cerr << "Failed to update SHA3-256 hash." << std::endl;
    EVP_MD_CTX_free(mdctx);
    return;
  }

  if (EVP_DigestFinal_ex(mdctx, hash, nullptr) != 1)
  {
    std::cerr << "Failed to finalize SHA3-256 hash." << std::endl;
    EVP_MD_CTX_free(mdctx);
    return;
  }

  EVP_MD_CTX_free(mdctx);

  std::vector<unsigned char> hash_vec(hash, hash + hash_size);
  {
    std::lock_guard<std::mutex> lock(queue_mutex[worker_id]);
    hash_queue[worker_id].push({op, hash_vec});
  }
  queue_cv[worker_id].notify_one();
}

void worker(char *page_buffer, vector<OpCode> ops, size_t num_threads, size_t worker_id)
{
  for (size_t off = 0; off < ops.size(); off += num_threads) // Stripe across workers
  {
    size_t i = off + worker_id;
    if (i < ops.size()) 
    {
      hash_page(page_buffer + i * pg_size, ops[i], worker_id);
    }
  }
  {
    std::lock_guard<std::mutex> lock(queue_mutex[worker_id]);
    active_workers--;

    if (active_workers == 0)
    {
      done = true;
    }
  }
  queue_cv[worker_id].notify_one();
}

void process_hashes(struct dedup_config *dc, vector<OpCode> ops, int32_t *status, uint64_t *counter)
{
  size_t i = 0;
  size_t curr_worker = 0;
  while (i < ops.size())
  {
    DmInstr instr;
    {
      std::unique_lock<std::mutex> lock(queue_mutex[curr_worker]);
      queue_cv[curr_worker].wait(lock, [curr_worker] { return !hash_queue[curr_worker].empty() || done; });

      instr = hash_queue[curr_worker].front();
      hash_queue[curr_worker].pop();
      lock.unlock();
    }

    process_bio(dc, instr.op, instr.hash.data(), status, counter);
    
    curr_worker = (curr_worker + 1) % num_threads;
    i++;
  }
}

void hash_table_process(char *page_buffer, size_t num_threads, struct dedup_config *dc, vector<OpCode> ops, int32_t *status, uint64_t *counter)
{
  std::vector<std::thread> hash_compute_threads;
  active_workers = num_threads;
  done = false;

  for (size_t i = 0; i < num_threads; ++i)
  {
    hash_compute_threads.emplace_back(worker, page_buffer, ops, num_threads, i);
  }

  std::thread hash_table_thread(process_hashes, dc, ops, status, counter);

  for (auto &thread : hash_compute_threads)
  {
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
  ("fullness,f", boost::program_options::value<double>()->default_value(0.5), "fullness of hash table")
  ("nBenchRun,r", boost::program_options::value<uint64_t>()->default_value(4), "Number of bench run")
  ("trace,t", boost::program_options::value<string>()->default_value(""), "Real world trace to use for benchmark")
  ("pages,p", boost::program_options::value<string>()->default_value(""), "Binary file for random page data");
  boost::program_options::variables_map commandLineArgs;
  boost::program_options::store(boost::program_options::parse_command_line(argc, argv, programDescription), commandLineArgs);
  boost::program_options::notify(commandLineArgs);

  uint64_t n_page = commandLineArgs["nPage"].as<uint64_t>();
  double hash_table_fullness = commandLineArgs["fullness"].as<double>();
  uint64_t n_bench_run = commandLineArgs["nBenchRun"].as<uint64_t>();
  string trace = static_cast<string>(commandLineArgs["trace"].as<string>());
  string page_filename = static_cast<string>(commandLineArgs["pages"].as<string>());

  struct dedup_config *dc;
  uint64_t data_size = 32768 * 8; // number of pblocks(unique pages) to manage
  dc = (dedup_config *)malloc(sizeof(*dc));
  if (dc == nullptr)
  {
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

  uint64_t total_page_unique_count = (((uint32_t) (hash_table_fullness * dedupSys::node_ht_size) + 15)/16) * 16;
  vector<uint32_t> ne_read_pages;
  vector<Instr> trace_instrs;
  loadTrace(trace, ne_read_pages, trace_instrs);

  std::cout << "Config: "<< endl;
  std::cout << "1. number of non-existent read pages to fill up: " << ne_read_pages.size() << endl;
  std::cout << "2. total number of pages in benchmark: " << total_page_unique_count << endl;

  char *all_unique_page_buffer = (char *)malloc(total_page_unique_count * pg_size);
  assert(all_unique_page_buffer != NULL);

  readFile(page_filename, all_unique_page_buffer, total_page_unique_count * pg_size);

  /* Step 1: Insert non-existent read pages */
  int32_t op_status;
  uint64_t step1_counter = 0;

  vector<OpCode> ops(ne_read_pages.size(), WRITE);
  char *ne_reads_buffer = (char *) malloc(ne_read_pages.size() * pg_size);
  assert(ne_reads_buffer != NULL);
  for (auto i = 0; i < ne_read_pages.size(); i++) {
    memcpy(ne_reads_buffer + i * pg_size, all_unique_page_buffer + ne_read_pages[i] * pg_size, pg_size);
  }

  std::cout << "Initializing hash table...... " << std::endl;

  auto begin_time = std::chrono::high_resolution_clock::now();
  hash_table_process(ne_reads_buffer, num_threads, dc, ops, &op_status, &step1_counter);
  auto end_time = std::chrono::high_resolution_clock::now();
  double time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - begin_time).count();

  std::cout << "Time used: " << time << "ns" << std::endl;
  std::cout << "Pages processed: " << step1_counter << "/" << ne_read_pages.size() << std::endl;

  /* Step 2: Benchmark */
  uint64_t step2_counter = 0;

  vector<OpCode> bench_ops(trace_instrs.size());
  char *benchmark_page_buffer = (char *) malloc(trace_instrs.size() * pg_size);
  assert(benchmark_page_buffer != NULL);
  for (size_t i = 0; i < trace_instrs.size(); i++) {
    bench_ops[i] = trace_instrs[i].opcode;
    memcpy(benchmark_page_buffer + i * pg_size, all_unique_page_buffer + trace_instrs[i].pg_idx_lst[0] * pg_size, pg_size);
  }
  
  std::cout << "Benchmarking...... " << std::endl;

  auto bench_begin_time = std::chrono::high_resolution_clock::now();
  hash_table_process(benchmark_page_buffer, num_threads, dc, bench_ops, &op_status, &step2_counter);
  auto bench_end_time = std::chrono::high_resolution_clock::now();
  double bench_time = std::chrono::duration_cast<std::chrono::nanoseconds>(bench_end_time - bench_begin_time).count();

  std::cout << "Time used: " << bench_time / 1000000 << "ms" << std::endl;
  std::cout << "kIOPS: " << ((double) trace_instrs.size()) / (bench_time / 1000000) << endl;
  std::cout << "GB/s: " << ((double) trace_instrs.size() * pg_size) / bench_time << endl;
  std::cout << "Pages processed: " << step2_counter << "/" << trace_instrs.size() << std::endl;

  // destructor
  dm_dedup_dtr(dc);
  return 0;
}