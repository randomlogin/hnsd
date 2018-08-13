#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "bn.h"
#include "chain.h"
#include "constants.h"
#include "error.h"
#include "header.h"
#include "map.h"
#include "msg.h"
#include "timedata.h"
#include "utils.h"

/*
 * Prototypes
 */

static int
hsk_chain_init_genesis(hsk_chain_t *chain);

static int
hsk_chain_insert(
  hsk_chain_t *chain,
  hsk_header_t *hdr,
  const hsk_header_t *prev
);

static void
hsk_chain_maybe_sync(hsk_chain_t *chain);

/*
 * Helpers
 */

static int
qsort_cmp(const void *a, const void *b) {
  int64_t x = *((int64_t *)a);
  int64_t y = *((int64_t *)b);

  if (x < y)
    return -1;

  if (x > y)
    return 1;

  return 0;
}

/*
 * Chain
 */

int
hsk_chain_init(hsk_chain_t *chain, const hsk_timedata_t *td) {
  if (!chain || !td)
    return HSK_EBADARGS;

  chain->height = 0;
  chain->tip = NULL;
  chain->genesis = NULL;
  chain->synced = false;
  chain->td = (hsk_timedata_t *)td;

  hsk_map_init_hash_map(&chain->hashes, free);
  hsk_map_init_int_map(&chain->heights, NULL);
  hsk_map_init_hash_map(&chain->orphans, free);
  hsk_map_init_hash_map(&chain->prevs, NULL);

  return hsk_chain_init_genesis(chain);
}

static int
hsk_chain_init_genesis(hsk_chain_t *chain) {
  if (!chain)
    return HSK_EBADARGS;

  hsk_header_t *tip = hsk_header_alloc();

  if (!tip)
    return HSK_ENOMEM;

  uint8_t *data = (uint8_t *)HSK_GENESIS;
  size_t size = sizeof(HSK_GENESIS) - 1;

  assert(hsk_header_decode(data, size, tip));
  assert(hsk_header_calc_work(tip, NULL));

  if (!hsk_map_set(&chain->hashes, hsk_header_cache(tip), tip)) {
    free(tip);
    return HSK_ENOMEM;
  }

  if (!hsk_map_set(&chain->heights, &tip->height, tip)) {
    hsk_map_del(&chain->hashes, hsk_header_cache(tip));
    free(tip);
    return HSK_ENOMEM;
  }

  chain->height = tip->height;
  chain->tip = tip;
  chain->genesis = tip;

  hsk_chain_maybe_sync(chain);

  return HSK_SUCCESS;
}

void
hsk_chain_uninit(hsk_chain_t *chain) {
  if (!chain)
    return;

  hsk_map_uninit(&chain->heights);
  hsk_map_uninit(&chain->hashes);
  hsk_map_uninit(&chain->prevs);
  hsk_map_uninit(&chain->orphans);

  chain->tip = NULL;
  chain->genesis = NULL;
}

hsk_chain_t *
hsk_chain_alloc(const hsk_timedata_t *td) {
  hsk_chain_t *chain = malloc(sizeof(hsk_chain_t));

  if (!chain)
    return NULL;

  if (hsk_chain_init(chain, td) != HSK_SUCCESS) {
    free(chain);
    return NULL;
  }

  return chain;
}

void
hsk_chain_free(hsk_chain_t *chain) {
  if (!chain)
    return;

  hsk_chain_uninit(chain);
  free(chain);
}

static void
hsk_chain_log(const hsk_chain_t *chain, const char *fmt, ...) {
  printf("chain (%u): ", (uint32_t)chain->height);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

bool
hsk_chain_has(const hsk_chain_t *chain, const uint8_t *hash) {
  return hsk_map_has(&chain->hashes, hash);
}

hsk_header_t *
hsk_chain_get(const hsk_chain_t *chain, const uint8_t *hash) {
  return hsk_map_get(&chain->hashes, hash);
}

hsk_header_t *
hsk_chain_get_by_height(const hsk_chain_t *chain, uint32_t height) {
  return hsk_map_get(&chain->heights, &height);
}

bool
hsk_chain_has_orphan(const hsk_chain_t *chain, const uint8_t *hash) {
  return hsk_map_has(&chain->orphans, hash);
}

hsk_header_t *
hsk_chain_get_orphan(const hsk_chain_t *chain, const uint8_t *hash) {
  return hsk_map_get(&chain->orphans, hash);
}

const uint8_t *
hsk_chain_safe_root(const hsk_chain_t *chain) {
  // The tree is committed on an interval.
  // Mainnet is 72 blocks, meaning at height 72,
  // the name set of the past 72 blocks are
  // inserted into the tree. The commitment for
  // that insertion actually appears in a block
  // header one block later (height 73). We
  // want the the root _before_ the current one
  // so we can calculate that with:
  //   chain_height - (chain_height % interval)

  uint32_t interval = HSK_TREE_INTERVAL;
  uint32_t mod = (uint32_t)chain->height % interval;

  // If there's enough proof-of-work
  // on top of the most recent root,
  // it should be safe to use it.
  if (mod >= 12)
    mod = 0;

  uint32_t height = (uint32_t)chain->height - mod;

  hsk_header_t *prev = hsk_chain_get_by_height(chain, height);
  assert(prev);

  hsk_chain_log(chain,
    "using safe height of %u for resolution\n",
    height);

  return prev->name_root;
}

static hsk_header_t *
hsk_chain_resolve_orphan(hsk_chain_t *chain, const uint8_t *hash) {
  hsk_header_t *orphan = hsk_map_get(&chain->prevs, hash);

  if (!orphan)
    return NULL;

  hsk_map_del(&chain->prevs, orphan->prev_block);
  hsk_map_del(&chain->orphans, hsk_header_cache(orphan));

  return orphan;
}

hsk_header_t *
hsk_chain_get_ancestor(
  const hsk_chain_t *chain,
  const hsk_header_t *hdr,
  uint32_t height
) {
  assert(height >= 0);
  assert(height <= hdr->height);

  hsk_header_t *h = (hsk_header_t *)hdr;

  while (h->height != height) {
    h = hsk_map_get(&chain->hashes, h->prev_block);
    assert(h);
  }

  return h;
}

static bool
hsk_chain_has_work(const hsk_chain_t *chain) {
  return memcmp(chain->tip->work, HSK_CHAINWORK, 32) >= 0;
}

static void
hsk_chain_maybe_sync(hsk_chain_t *chain) {
  if (chain->synced)
    return;

  int64_t now = hsk_timedata_now(chain->td);

  if (now < HSK_LAUNCH_DATE) {
    hsk_chain_log(chain, "chain is fully synced\n");
    chain->synced = true;
    return;
  }

  if (HSK_USE_CHECKPOINTS) {
    if (chain->height < HSK_LAST_CHECKPOINT)
      return;
  }

  if (((int64_t)chain->tip->time) < now - HSK_MAX_TIP_AGE)
    return;

  if (!hsk_chain_has_work(chain))
    return;

  hsk_chain_log(chain, "chain is fully synced\n");
  chain->synced = true;
}

bool
hsk_chain_synced(const hsk_chain_t *chain) {
  return chain->synced;
}

void
hsk_chain_get_locator(const hsk_chain_t *chain, hsk_getheaders_msg_t *msg) {
  assert(chain && msg);

  int i = 0;
  hsk_header_t *tip = chain->tip;
  int64_t height = chain->height;
  int64_t step = 1;

  hsk_header_hash(tip, msg->hashes[i++]);

  while (height > 0) {
    height -= step;

    if (height < 0)
      height = 0;

    if (i > 10)
      step *= 2;

    if (i == sizeof(msg->hashes) - 1)
      height = 0;

    hsk_header_t *hdr = hsk_chain_get_by_height(chain, (uint32_t)height);
    assert(hdr);

    hsk_header_hash(hdr, msg->hashes[i++]);
  }

  msg->hash_count = i;
}

static int64_t
hsk_chain_get_mtp(const hsk_chain_t *chain, const hsk_header_t *prev) {
  assert(chain);

  if (!prev)
    return 0;

  int timespan = 11;
  int64_t median[11];
  size_t size = 0;
  int i;

  for (i = 0; i < timespan && prev; i++) {
    median[i] = (int64_t)prev->time;
    prev = hsk_map_get(&chain->hashes, prev->prev_block);
    size += 1;
  }

  qsort((void *)median, size, sizeof(int64_t), qsort_cmp);

  return median[size >> 1];
}

static uint32_t
hsk_chain_retarget(const hsk_chain_t *chain, const hsk_header_t *prev) {
  assert(chain);

  uint32_t bits = HSK_BITS;
  uint8_t *limit = (uint8_t *)HSK_LIMIT;

  int64_t window = HSK_TARGET_WINDOW;
  int64_t timespan = HSK_TARGET_TIMESPAN;
  int64_t min = HSK_MIN_ACTUAL;
  int64_t max = HSK_MAX_ACTUAL;

  if (!prev)
    return bits;

  hsk_bn_t target_bn;
  hsk_bn_init(&target_bn);

  hsk_header_t *last = (hsk_header_t *)prev;
  hsk_header_t *first = last;

  int64_t i;
  for (i = 0; first && i < window; i++) {
    uint8_t diff[32];
    assert(hsk_pow_to_target(first->bits, diff));
    hsk_bn_t diff_bn;
    hsk_bn_from_array(&diff_bn, diff, 32);
    hsk_bn_add(&target_bn, &diff_bn, &target_bn);
    first = hsk_map_get(&chain->hashes, first->prev_block);
  }

  if (!first || first->height < 1)
    return bits;

  hsk_bn_t window_bn;
  hsk_bn_from_int(&window_bn, (uint64_t)window);

  hsk_bn_div(&target_bn, &window_bn, &target_bn);

  int64_t start = hsk_chain_get_mtp(chain, first);
  int64_t end = hsk_chain_get_mtp(chain, last);
  int64_t diff = end - start;
  int64_t actual = timespan + ((diff - timespan) >> 2);

  if (actual < min)
    actual = min;

  if (actual > max)
    actual = max;

  hsk_bn_t actual_bn;
  hsk_bn_from_int(&actual_bn, (uint64_t)actual);

  hsk_bn_t timespan_bn;
  hsk_bn_from_int(&timespan_bn, (uint64_t)timespan);

  hsk_bn_div(&target_bn, &timespan_bn, &target_bn);
  hsk_bn_mul(&target_bn, &actual_bn, &target_bn);

  hsk_bn_t limit_bn;
  hsk_bn_from_array(&limit_bn, limit, 32);

  if (hsk_bn_cmp(&target_bn, &limit_bn) > 0)
    return bits;

  uint8_t target[32];
  hsk_bn_to_array(&target_bn, target, 32);

  uint32_t cmpct;

  assert(hsk_pow_to_bits(target, &cmpct));

  return cmpct;
}

static uint32_t
hsk_chain_get_target(
  const hsk_chain_t *chain,
  int64_t time,
  const hsk_header_t *prev
) {
  assert(chain);

  // Genesis
  if (!prev) {
    assert(time == chain->genesis->time);
    return HSK_BITS;
  }

  if (HSK_NO_RETARGETTING)
    return HSK_BITS;

  if (HSK_TARGET_RESET) {
    // Special behavior for testnet:
    if (time > (int64_t)prev->time + HSK_TARGET_SPACING * 4)
      return HSK_BITS;
   }

  return hsk_chain_retarget(chain, prev);
}

static hsk_header_t *
hsk_chain_find_fork(
  const hsk_chain_t *chain,
  hsk_header_t *fork,
  hsk_header_t *longer
) {
  assert(chain && fork && longer);

  while (!hsk_header_equal(fork, longer)) {
    while (longer->height > fork->height) {
      longer = hsk_map_get(&chain->hashes, longer->prev_block);
      if (!longer)
        return NULL;
    }

    if (hsk_header_equal(fork, longer))
      return fork;

    fork = hsk_map_get(&chain->hashes, fork->prev_block);

    if (!fork)
      return NULL;
  }

  return fork;
}

static void
hsk_chain_reorganize(hsk_chain_t *chain, hsk_header_t *competitor) {
  assert(chain && competitor);

  hsk_header_t *tip = chain->tip;
  hsk_header_t *fork = hsk_chain_find_fork(chain, tip, competitor);

  assert(fork);

  // Blocks to disconnect.
  hsk_header_t *disconnect = NULL;
  hsk_header_t *entry = tip;
  hsk_header_t *tail = NULL;
  while (!hsk_header_equal(entry, fork)) {
    assert(!entry->next);

    if (!disconnect)
      disconnect = entry;

    if (tail)
      tail->next = entry;

    tail = entry;

    entry = hsk_map_get(&chain->hashes, entry->prev_block);
    assert(entry);
  }

  // Blocks to connect.
  entry = competitor;
  hsk_header_t *connect = NULL;
  while (!hsk_header_equal(entry, fork)) {
    assert(!entry->next);

    // Build the list backwards.
    if (connect)
      entry->next = connect;

    connect = entry;

    entry = hsk_map_get(&chain->hashes, entry->prev_block);
    assert(entry);
  }

  // Disconnect blocks.
  hsk_header_t *c, *n;
  for (c = disconnect; c; c = n) {
    n = c->next;
    c->next = NULL;
    hsk_map_del(&chain->heights, &c->height);
  }

  // Connect blocks (backwards, save last).
  for (c = connect; c; c = n) {
    n = c->next;
    c->next = NULL;

    if (!n) // halt on last
      break;

    assert(hsk_map_set(&chain->heights, &c->height, (void *)c));
  }
}

int
hsk_chain_add(hsk_chain_t *chain, const hsk_header_t *h) {
  if (!chain || !h)
    return HSK_EBADARGS;

  int rc = HSK_SUCCESS;
  hsk_header_t *hdr = hsk_header_clone(h);

  if (!hdr) {
    rc = HSK_ENOMEM;
    goto fail;
  }

  const uint8_t *hash = hsk_header_cache(hdr);

  hsk_chain_log(chain, "adding block: %s\n", hsk_hex_encode32(hash));

  int64_t now = hsk_timedata_now(chain->td);

  if (hdr->time > now + 2 * 60 * 60) {
    hsk_chain_log(chain, "  rejected: time-too-new\n");
    rc = HSK_ETIMETOONEW;
    goto fail;
  }

  if (hsk_map_has(&chain->hashes, hash)) {
    hsk_chain_log(chain, "  rejected: duplicate\n");
    rc = HSK_EDUPLICATE;
    goto fail;
  }

  if (hsk_map_has(&chain->orphans, hash)) {
    hsk_chain_log(chain, "  rejected: duplicate-orphan\n");
    rc = HSK_EDUPLICATEORPHAN;
    goto fail;
  }

  rc = hsk_header_verify_pow(hdr);

  if (rc != HSK_SUCCESS) {
    hsk_chain_log(chain, "  rejected: pow error: %s\n", hsk_strerror(rc));
    goto fail;
  }

  hsk_header_t *prev = hsk_chain_get(chain, hdr->prev_block);

  if (!prev) {
    hsk_chain_log(chain, "  stored as orphan\n");

    if (chain->orphans.size > 10000) {
      hsk_chain_log(chain, "clearing orphans: %d\n", chain->orphans.size);
      hsk_map_clear(&chain->prevs);
      hsk_map_clear(&chain->orphans);
    }

    if (!hsk_map_set(&chain->orphans, hash, (void *)hdr)) {
      rc = HSK_ENOMEM;
      goto fail;
    }

    if (!hsk_map_set(&chain->prevs, hdr->prev_block, (void *)hdr)) {
      hsk_map_del(&chain->orphans, hash);
      rc = HSK_ENOMEM;
      goto fail;
    }

    return HSK_EORPHAN;
  }

  rc = hsk_chain_insert(chain, hdr, prev);

  if (rc != HSK_SUCCESS)
    goto fail;

  for (;;) {
    prev = hdr;
    hdr = hsk_chain_resolve_orphan(chain, hash);

    if (!hdr)
      break;

    hash = hsk_header_cache(hdr);

    rc = hsk_chain_insert(chain, hdr, prev);

    hsk_chain_log(chain, "resolved orphan: %s\n", hsk_hex_encode32(hash));

    if (rc != HSK_SUCCESS) {
      free(hdr);
      return rc;
    }
  }

  return rc;

fail:
  if (hdr)
    free(hdr);

  return rc;
}

static int
hsk_chain_insert(
  hsk_chain_t *chain,
  hsk_header_t *hdr,
  const hsk_header_t *prev
) {
  const uint8_t *hash = hsk_header_cache(hdr);
  int64_t mtp = hsk_chain_get_mtp(chain, prev);

  if ((int64_t)hdr->time <= mtp) {
    hsk_chain_log(chain, "  rejected: time-too-old\n");
    return HSK_ETIMETOOOLD;
  }

  uint32_t bits = hsk_chain_get_target(chain, hdr->time, prev);

  if (hdr->bits != bits) {
    hsk_chain_log(chain,
      "  rejected: bad-diffbits: %x != %x\n",
      hdr->bits, bits);
    return HSK_EBADDIFFBITS;
  }

  hdr->height = prev->height + 1;

  assert(hsk_header_calc_work(hdr, prev));

  if (memcmp(hdr->work, chain->tip->work, 32) <= 0) {
    if (!hsk_map_set(&chain->hashes, hash, (void *)hdr))
      return HSK_ENOMEM;

    hsk_chain_log(chain, "  stored on alternate chain\n");
  } else {
    if (memcmp(hdr->prev_block, hsk_header_cache(chain->tip), 32) != 0) {
      hsk_chain_log(chain, "  reorganizing...\n");
      hsk_chain_reorganize(chain, hdr);
    }

    if (!hsk_map_set(&chain->hashes, hash, (void *)hdr))
      return HSK_ENOMEM;

    if (!hsk_map_set(&chain->heights, &hdr->height, (void *)hdr)) {
      hsk_map_del(&chain->hashes, hash);
      return HSK_ENOMEM;
    }

    chain->height = hdr->height;
    chain->tip = hdr;

    hsk_chain_log(chain, "  added to main chain\n");
    hsk_chain_log(chain, "  new height: %u\n", (uint32_t)chain->height);

    hsk_chain_maybe_sync(chain);
  }

  return HSK_SUCCESS;
}
