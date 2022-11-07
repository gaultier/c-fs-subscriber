#include <_types/_uint8_t.h>
#include <sys/_types/_uintptr_t.h>
#include <unistd.h>

#include "bencode.h"
#include "peer.h"
#include "tracker.h"
#include "uv.h"
#include "vendor/greatest/greatest.h"

static uint8_t peer_id[20] = {0};
static uint8_t info_hash[20] = {0};
const uint8_t handshake_header[] = {
    PEER_HANDSHAKE_HEADER_LENGTH,
    'B',
    'i',
    't',
    'T',
    'o',
    'r',
    'r',
    'e',
    'n',
    't',
    ' ',
    'p',
    'r',
    'o',
    't',
    'o',
    'c',
    'o',
    'l',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

TEST test_read_bufs() {
  pg_logger_t logger = {.level = PG_LOG_DEBUG};

  pg_pool_t peer_pool = {0};
  pg_pool_init(&peer_pool, sizeof(peer_t), 1);

  pg_array_t(uint8_t) pieces = {0};
  pg_array_init_reserve(pieces, 2 * 20, pg_heap_allocator());
  pg_array_resize(pieces, 2 * 20);
  bc_metainfo_t metainfo = {.announce = "http://localhost",
                            .length = 3 * PEER_BLOCK_LENGTH + 1,
                            .piece_length = 2 * PEER_BLOCK_LENGTH,
                            .pieces = pieces,
                            .name = "foo"};

  download_t download = {0};
  download_init(pg_heap_allocator(), &download, &metainfo, info_hash, peer_id,
                0);

  const tracker_peer_address_t addr = {0};

  peer_t* peer = pg_pool_alloc(&peer_pool);
  assert(peer != NULL);
  peer_init(peer, &logger, &peer_pool, &download, &metainfo, addr);

  uv_buf_t buf1 = {0};
  peer_alloc((uv_handle_t*)&peer->connection, 65536, &buf1);
  ASSERT(buf1.base != NULL);

  peer_read_buf_t* read_buf = (peer_read_buf_t*)((uintptr_t*)buf1.base - 2);
  ASSERT(read_buf != NULL);

  memcpy(buf1.base, handshake_header, sizeof(handshake_header));
  ASSERT_EQ_FMT((uint8_t)PEER_HANDSHAKE_HEADER_LENGTH, read_buf->data[0], "%d");

  memcpy(buf1.base + sizeof(handshake_header), peer->download->info_hash,
         sizeof(peer->download->info_hash));
  memcpy(
      buf1.base + sizeof(handshake_header) + sizeof(peer->download->info_hash),
      peer->download->peer_id, sizeof(peer->download->peer_id));
  buf1.len = PEER_HANDSHAKE_LENGTH;

  uv_stream_t stream = {.data = peer};
  peer_on_read(&stream, PEER_HANDSHAKE_LENGTH, &buf1);

  ASSERT(peer->read_bufs_start == NULL);
  ASSERT(peer->read_bufs_start == peer->read_bufs_end);
  ASSERT(peer->read_bufs_start->next == NULL);
  ASSERT(peer->read_bufs_end->next == NULL);
  ASSERT_EQ_FMT(5ULL, peer->read_bufs_start->len, "%llu");
  ASSERT_STRN_EQ("Hello", peer->read_bufs_start->data,
                 peer->read_bufs_start->len);

  PASS();
}

GREATEST_MAIN_DEFS();

int main(int argc, char** argv) {
  GREATEST_MAIN_BEGIN(); /* command-line options, initialization. */

  RUN_TEST(test_read_bufs);

  GREATEST_MAIN_END(); /* display results */
}
