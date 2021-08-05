// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.

  struct bucket buckets[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; ++i) {
    initlock(&bcache.buckets[i].lock, "bache.bucket");
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Create linked list of buffers in bucket 0
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bucketno = blockno % NBUCKET;
  acquire(&bcache.buckets[bucketno].lock);

  // Is the block already cached?
  for(b = bcache.buckets[bucketno].head.next; b != &bcache.buckets[bucketno].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.buckets[bucketno].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  uint min_timestamp = (1L << 32) - 1, lru_bukno = 0;
  struct buf *lru_buf = 0;
  acquire(&bcache.lock);
  for (int i = 0; i < NBUCKET; ++i) {
    if (i != bucketno) {
      acquire(&bcache.buckets[i].lock);
    }

    for (b = bcache.buckets[i].head.next; b != &bcache.buckets[i].head; b = b->next) {
      if (b->refcnt == 0 && b->timestamp < min_timestamp) {
        lru_buf = b;
        lru_bukno = i;
        min_timestamp = b->timestamp;
      }
    }
    
    if (i != bucketno) {
      release(&bcache.buckets[i].lock);
    }
  }

  if (lru_buf) {
    if (lru_bukno != bucketno) {
      acquire(&bcache.buckets[lru_bukno].lock);
      lru_buf->prev->next = lru_buf->next;
      lru_buf->next->prev = lru_buf->prev;

      lru_buf->next = bcache.buckets[bucketno].head.next;
      lru_buf->prev = &bcache.buckets[bucketno].head;
      bcache.buckets[bucketno].head.next->prev = lru_buf;
      bcache.buckets[bucketno].head.next = lru_buf;
    }

    lru_buf->dev = dev;
    lru_buf->blockno = blockno;
    lru_buf->valid = 0;
    lru_buf->refcnt = 1;

    if (lru_bukno != bucketno) {
      release(&bcache.buckets[lru_bukno].lock);
    }

    release(&bcache.lock);
    release(&bcache.buckets[bucketno].lock);
    acquiresleep(&lru_buf->lock);
    return lru_buf;
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint bucketno = b->blockno % NBUCKET;
  acquire(&bcache.buckets[bucketno].lock);
  b->refcnt--;
  b->timestamp = ticks;
  release(&bcache.buckets[bucketno].lock);
}

void
bpin(struct buf *b) {
  uint bucketno = b->blockno % NBUCKET;
  acquire(&bcache.buckets[bucketno].lock);
  b->refcnt++;
  release(&bcache.buckets[bucketno].lock);
}

void
bunpin(struct buf *b) {
  uint bucketno = b->blockno % NBUCKET;
  acquire(&bcache.buckets[bucketno].lock);
  b->refcnt--;
  release(&bcache.buckets[bucketno].lock);
}


