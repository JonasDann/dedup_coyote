#include "dmdedup.hpp"
#include <stdint.h>
#include <iostream>
#include <malloc.h>
#include <stdexcept>
#include <cstring>
#include <cstdio>

// #include <linux/vmalloc.h>
// #include <linux/errno.h>


#define EMPTY_ENTRY 0xFF
#define DELETED_ENTRY 0x6B

#define UINT32_MAX	(4294967295U)
#define HASHTABLE_OVERPROV	(10)

namespace dmdedup {

void mem_print(const void* mem, size_t length) {
    // const unsigned char* bytes = static_cast<const unsigned char*>(mem);
    // std::ostringstream stream;

    // for (size_t i = 0; i < length; ++i) {
    //     stream << std::hex << std::setw(2) << std::setfill('0')
    //            << static_cast<int>(bytes[i]);
    // }

    // std::cout << stream.str() << std::endl;
  const unsigned char* bytes = static_cast<const unsigned char*>(mem);

  // for (size_t i = 0; i < length; ++i) {
  // 		printf("%02x", bytes[i]);
  // }
  for (size_t i = 0; i < length; ++i) {
      printf("%02x", bytes[length - 1 - i]);
  }

  printf("\n");
}

static struct metadata *init_meta_inram(void *init_param, bool *unformatted)
{
  uint64_t smap_size;
  struct metadata *md;
  struct init_param_inram *p = (struct init_param_inram *)init_param;

  std::cout << "Initializing INRAM backend" << std::endl;

  *unformatted = true;

  // md = kmalloc(sizeof(*md), GFP_NOIO);
  // if (!md)
  // 	return ERR_PTR(-ENOMEM);
  md = (metadata*)malloc(sizeof(*md));
  if (md == NULL) {
    // Handle memory allocation failure
    // throw std::runtime_error("invalid pointer");
    return NULL;
  }

  smap_size = static_cast<uint64_t>(p->blocks) * sizeof(uint32_t);

  md->smap = (uint32_t*)malloc(smap_size);
  if (md->smap == NULL) {
    free(md);
    // throw std::runtime_error("invalid pointer");
    return NULL;
  }

  uint64_t tmp = smap_size / (1024 * 1024);
  uint64_t remainingBytes = smap_size % (1024 * 1024);
  std::cout << "Space allocated for pbn reference count map: " << tmp << "." << remainingBytes << " MB\n";

  memset(md->smap, 0, smap_size);

  md->smax = p->blocks;
  md->allocptr = 0;
  // md->kvs_linear = NULL;
  md->kvs_sparse = NULL;

  return md;
}

static void exit_meta_inram(struct metadata *md)
{
  if (md->smap != NULL)
    free(md->smap);

  // if (md->kvs_linear) {
  // 	if (md->kvs_linear->store)
  // 		vfree(md->kvs_linear->store);
  // 	kfree(md->kvs_linear);
  // }

  if (md->kvs_sparse) {
    if (md->kvs_sparse->store)
      free(md->kvs_sparse->store);
    free(md->kvs_sparse);
  }

  free(md);
}

static int flush_meta_inram(struct metadata *md)
{
  return 0;
}

/********************************************************
 *		Space Management Functions		*
 ********************************************************/

static uint64_t next_head(uint64_t current_head, uint64_t smax)
{
  current_head += 1;
  return current_head%smax;
}

static int alloc_data_block_inram(struct metadata *md, uint64_t *blockn)
{
  uint64_t head, tail;

  head = md->allocptr;
  tail = md->allocptr;

  do {
    if (!md->smap[head]) {
      md->smap[head] = 1;
      *blockn = head;
      md->allocptr = next_head(head, md->smax);
      return 0;
    }

    head = next_head(head, md->smax);

  } while (head != tail);

  return -ENOSPC;
}

static int inc_refcount_inram(struct metadata *md, uint64_t blockn)
{
  if (blockn >= md->smax)
    return -ERANGE;

  if (md->smap[blockn] != UINT32_MAX)
    md->smap[blockn]++;
  else
    return -E2BIG;

  return 0;
}

static int dec_refcount_inram(struct metadata *md, uint64_t blockn)
{
  if (blockn >= md->smax)
    return -ERANGE;

  if (md->smap[blockn])
    md->smap[blockn]--;
  else
    return -EFAULT;

  return 0;
}

static int get_refcount_inram(struct metadata *md, uint64_t blockn)
{
  if (blockn >= md->smax)
    return -ERANGE;

  return md->smap[blockn];
}

/********************************************************
 *		General KVS Functions			*
 ********************************************************/

static bool is_empty(char *ptr, int length)
{
  int i = 0;
  unsigned char* ptr_new = (unsigned char*) ptr;
  while ((i < length) && (ptr_new[i] == EMPTY_ENTRY))
    i++;

  return i == length;
}

static bool is_deleted(char *ptr, int length)
{
  int i = 0;
  unsigned char* ptr_new = (unsigned char*) ptr;
  while ((i < length) && (ptr_new[i] == DELETED_ENTRY))
    i++;

  return i == length;
}

/********************************************************
 *		Sparse KVS Functions			*
 ********************************************************/

static int kvs_delete_sparse_inram(struct kvstore *kvs,
           void *key, int32_t ksize)
{
  uint64_t idxhead = *((uint64_t *)key);
  uint32_t entry_size, head, tail;
  char *ptr;
  struct kvstore_inram *kvinram = NULL;

  if (ksize != kvs->ksize)
    return -EINVAL;

  // kvinram = container_of(kvs, struct kvstore_inram, ckvs);
  kvinram = reinterpret_cast<kvstore_inram*>(
        reinterpret_cast<char*>(kvs) - offsetof(kvstore_inram, ckvs));

  entry_size = kvs->vsize + kvs->ksize;
  // head = do_div(idxhead, kvinram->kmax);
  head = idxhead % kvinram->kmax;  // Replacing do_div
  tail = head;

  do {
    ptr = kvinram->store + entry_size * head;

    if (is_empty(ptr, entry_size))
      goto doesnotexist;

    if (memcmp(ptr, key, kvs->ksize)) {
      head = next_head(head, kvinram->kmax);
    } else {
      memset(ptr, DELETED_ENTRY, entry_size);
      return 0;
    }
  } while (head != tail);

doesnotexist:
  return -ENODEV;
}

/*
 * 0 - if entry found
 * -ENODATA - if entry not found
 * < 0 - error on lookup
 */
static int kvs_lookup_sparse_inram(struct kvstore *kvs, void *key,
           int32_t ksize, void *value, int32_t *vsize)
{
  uint64_t idxhead = *((uint64_t *)key);
  uint32_t entry_size, head, tail;
  char *ptr;
  struct kvstore_inram *kvinram = NULL;
  int r = -ENODATA;

  if (ksize != kvs->ksize)
    return -EINVAL;

  // kvinram = container_of(kvs, struct kvstore_inram, ckvs);
  kvinram = reinterpret_cast<kvstore_inram*>(
        reinterpret_cast<char*>(kvs) - offsetof(kvstore_inram, ckvs));

  entry_size = kvs->vsize + kvs->ksize;
  // head = do_div(idxhead, kvinram->kmax);
  head = idxhead % kvinram->kmax;  // Replacing do_div
  tail = head;

  do {
    ptr = kvinram->store + entry_size * head;

    if (is_empty(ptr, entry_size)) {
      return r;
    }

    if (std::memcmp(ptr, key, kvs->ksize)) {
      // head = (head + 1) % kvinram->kmax;  // Replacing next_head
      head = next_head(head, kvinram->kmax);
    } else {
      std::memcpy(value, ptr + kvs->ksize, kvs->vsize);
      return 0;
    }

  } while (head != tail);

  return r;
}

static int kvs_insert_sparse_inram(struct kvstore *kvs, void *key,
           int32_t ksize, void *value, int32_t vsize)
{
  uint64_t idxhead = *((uint64_t *)key);
  uint32_t entry_size, head, tail;
  char *ptr;
  struct kvstore_inram *kvinram = NULL;

  // BUG_ON(!kvs);
  if (!kvs) {
      // Handle null pointer error
      throw std::runtime_error("insert null pointer error");
  }

  if (ksize > kvs->ksize)
    return -EINVAL;

  // kvinram = container_of(kvs, struct kvstore_inram, ckvs);
  kvinram = reinterpret_cast<kvstore_inram*>(
        reinterpret_cast<char*>(kvs) - offsetof(kvstore_inram, ckvs));

  entry_size = kvs->vsize + kvs->ksize;
  // head = do_div(idxhead, kvinram->kmax);
  head = idxhead % kvinram->kmax;
  tail = head;

  do {
    ptr = kvinram->store + entry_size * head;

    if (is_empty(ptr, entry_size) || is_deleted(ptr, entry_size)) {
      memcpy(ptr, key, kvs->ksize);
      memcpy(ptr + kvs->ksize, value, kvs->vsize);
      // std::cout << "input key size: " << kvs->ksize << std::endl;
      // mem_print(key, kvs->ksize);
      // std::cout << "input value: " << *(uint64_t*)value <<" size: " << kvs->vsize << std::endl;
      // mem_print(value, kvs->vsize);
      // std::cout << "insert key: ";
      // mem_print(ptr, kvs->ksize);
      // std::cout << "insert value: ";
      // mem_print(ptr + kvs->ksize, kvs->vsize);
      return 0;
    }

    head = next_head(head, kvinram->kmax);

  } while (head != tail);

  return -ENOSPC;
}

/*
 *
 * NOTE: if iteration_action() is a deletion/cleanup function,
 *	 Make sure that the store is implemented such that
 *	 deletion in-place is safe while iterating.
 */
static int kvs_iterate_sparse_inram(struct kvstore *kvs,
            int (*iteration_action)
            (void *key, int32_t ksize,
             void *value, int32_t vsize,
             void *data), void *data)
{
  int err = 0;
  uint32_t kvalue_size, head = 0;
  char *ptr = NULL;
  struct kvstore_inram *kvinram = NULL;

  // BUG_ON(!kvs);
  if (!kvs) {
      // Handle null pointer error
      throw std::runtime_error("insert null pointer error");
  }

  // kvinram = container_of(kvs, struct kvstore_inram, ckvs);
  kvinram = reinterpret_cast<kvstore_inram*>(
        reinterpret_cast<char*>(kvs) - offsetof(kvstore_inram, ckvs));
  // ptrdiff_t ckvs_offset = reinterpret_cast<char*>(
  //                          &reinterpret_cast<kvstore_inram*>(0)->ckvs)
  //                      - reinterpret_cast<char*>(0);

  // // Calculate the start address of kvstore_inram using the offset
  // kvinram = reinterpret_cast<kvstore_inram*>(reinterpret_cast<char*>(kvs) - ckvs_offset);

  kvalue_size = kvs->vsize + kvs->ksize;

  do {
    ptr = kvinram->store + (head * kvalue_size);
    // mem_print(ptr, kvalue_size);
    // std::cout << (!is_empty(ptr, kvalue_size) && !is_deleted(ptr, kvalue_size)) << std::endl;
    // std::cout << (((unsigned char * )ptr)[0] == EMPTY_ENTRY) << std::endl;
    // std::cout << ((unsigned char * )ptr)[0] << std::endl;
    if (!is_empty(ptr, kvalue_size) &&
        !is_deleted(ptr, kvalue_size)) {
      err = iteration_action((void *)ptr, kvs->ksize,
                 (void *)(ptr + kvs->ksize),
                 kvs->vsize, data);

      if (err < 0)
        goto out;
    }

    head = next_head(head, kvinram->kmax);
  } while (head);

out:
  return err;
}

static struct kvstore *kvs_create_sparse_inram(struct metadata *md,
                 uint32_t ksize, uint32_t vsize,
                 uint32_t knummax, bool unformatted)
{
  if (!vsize || !ksize || !knummax) {
    std::cerr << "Invalid size parameters" << std::endl;
    return nullptr;
  }
  /* We do not support two or more KVSs at the moment */
  if (md->kvs_sparse) {
    std::cerr << "Only one KVS is supported" << std::endl;
    return nullptr;
  }

  kvstore_inram* kvs = new kvstore_inram;
  if (!kvs) {
      return nullptr;
  }

  knummax += (knummax * HASHTABLE_OVERPROV) / 100;
  // uint64_t kvstore_size = static_cast<uint64_t>((knummax) * (vsize + ksize));
  uint64_t kvstore_size = (static_cast<uint64_t>(knummax)) * (static_cast<uint64_t>(vsize + ksize));
  // kvstore_size = (knummax * (vsize + ksize));

  kvs->store = (char*) malloc(kvstore_size);
  if (!kvs->store) {
    delete kvs;
    return nullptr;
  }

  uint64_t tmp = kvstore_size / (1024 * 1024);
  uint64_t remainingBytes = kvstore_size % (1024 * 1024);
  std::cout << "Space allocated for sparse key value store: " << tmp << "." << remainingBytes << " MB\n";

  memset(kvs->store, EMPTY_ENTRY, kvstore_size);
  // mem_print(kvs->store, kvstore_size);

  kvs->ckvs.vsize = vsize;
  kvs->ckvs.ksize = ksize;
  kvs->kmax = knummax;

  kvs->ckvs.kvs_insert = kvs_insert_sparse_inram;
  kvs->ckvs.kvs_lookup = kvs_lookup_sparse_inram;
  kvs->ckvs.kvs_delete = kvs_delete_sparse_inram;
  kvs->ckvs.kvs_iterate = kvs_iterate_sparse_inram;

  md->kvs_sparse = kvs;

  return &(kvs->ckvs);
}

struct metadata_ops metadata_ops_inram = {
  .init_meta = init_meta_inram,
  .exit_meta = exit_meta_inram,
  .kvs_create_sparse = kvs_create_sparse_inram,

  .alloc_data_block = alloc_data_block_inram,
  .inc_refcount = inc_refcount_inram,
  .dec_refcount = dec_refcount_inram,
  .get_refcount = get_refcount_inram,
};


}