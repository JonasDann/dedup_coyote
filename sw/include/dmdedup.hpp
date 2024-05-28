#pragma once

#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace dmdedup {

struct kvstore {
  uint32_t vsize;
  uint32_t ksize;

  int32_t (*kvs_delete)(struct kvstore *kvs, void *key, int32_t ksize);
  int32_t (*kvs_lookup)(struct kvstore *kvs, void *key, int32_t ksize,
        void *value, int32_t *vsize);
  int32_t (*kvs_insert)(struct kvstore *kvs, void *key, int32_t ksize,
        void *value, int32_t vsize);
  int32_t (*kvs_iterate)(struct kvstore *kvs, int32_t (*itr_action)
        (void *key, int32_t ksize, void *value,
         int32_t vsize, void *data), void *data);
};

struct kvstore_inram {
  struct kvstore ckvs;
  uint32_t kmax;
  char *store;
};

struct metadata {
  /* Space Map */
  uint32_t *smap;
  uint64_t smax;
  uint64_t allocptr;

  /*
   * XXX: Currently we support only one linear and one sparse KVS.
   */
  // struct kvstore_inram *kvs_linear;
  struct kvstore_inram *kvs_sparse;
};

/* Value of the HASH-PBN key-value store */
struct hash_pbn_value {
  uint64_t pbn;	/* in blocks */
};

struct init_param_inram {
  uint64_t blocks;
};

struct metadata_ops {
  /*
   * It initializes backend for cowbtree and inram. In case of cowbtree
   * either new metadata device is created or it is reconstructed from
   * existing metadata device. For in-ram backend new linked list is
   * initialized.
   *
   * Returns ERR_PTR(*) on error.
   * Valid pointer on success.
   */
  struct metadata * (*init_meta)(void *init_param, bool *unformatted);

  void (*exit_meta)(struct metadata *md);

  /*
   * Creates sparse key-value store. Ksize and vsize in bytes.
   * If ksize or vsize are equal to zero, it means that keys
   * and values will be of a variable size. knummax is the
   * maximum _number_ of the keys. If keymax is equal to zero,
   * then maximum is not known by the caller.
   *
   * Returns ERR_PTR(*) on error.
   * Valid pointer on success.
   */
  struct kvstore * (*kvs_create_sparse)(struct metadata *md,
                uint32_t ksize, uint32_t vsize,
                uint32_t knummax, bool unformatted);

  /*
   * Returns -ERR code on error.
   * Returns 0 on success. In this case, "blockn" contains a newly
   * allocated block number.
   */
  int32_t (*alloc_data_block)(struct metadata *md, uint64_t *blockn);

  /*
   * Returns -ERR code on error.
   * Returns 0 on success.
   */
  int32_t (*inc_refcount)(struct metadata *md, uint64_t blockn);

  /*
   * Returns -ERR code on error.
   * Returns 0 on success.
   */
  int32_t (*dec_refcount)(struct metadata *md, uint64_t blockn);

  /*
   * Returns -ERR code on error.
   * Returns refcount on success.
   */
  int32_t (*get_refcount)(struct metadata *md, uint64_t blockn);
};

extern struct metadata_ops metadata_ops_inram;

/* Per target instance structure */
struct dedup_config {
  uint32_t pblocks;	/* physical blocks */
  struct metadata_ops *mdops;
  struct metadata *bmd;
  struct kvstore *kvs_hash_pbn;

  int crypto_key_size;
};

void mem_print(const void* mem, size_t length);

}