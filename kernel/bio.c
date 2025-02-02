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

#define NBUCKETS 13

struct bucket {
  struct spinlock lock;
  struct buf* buf;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKETS];
} bcache;

void binit(void) {

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUF; i++) {
    if (i < 13) {
      initlock(&bcache.buckets[i].lock, "bcache_buckets_lock");
      bcache.buckets[i].buf = &bcache.buf[i];
    } else {
      int index = i % 13;
      struct buf *b = bcache.buckets[index].buf;

      while (b->next) {
        b = b->next;
      }

      b->next = &bcache.buf[i];
    }
    initsleeplock(&bcache.buf[i].lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {

  int index = blockno % NBUCKETS;
  acquire(&bcache.buckets[index].lock);
  struct buf *bf = bcache.buckets[index].buf;

  // Is the block already cached?
  while (bf) {
    if (bf->dev == dev && bf->blockno == blockno) {
      bf->refcnt++;
      release(&bcache.buckets[index].lock);
      acquiresleep(&bf->lock);
      return bf;
    }

    bf = bf->next;
  }

  // Not cached, find empty buf in this bucket;
  bf = bcache.buckets[index].buf;
  while (bf) {
    if (bf->refcnt == 0) {
      bf->dev = dev;
      bf->blockno = blockno;
      bf->valid = 0;
      bf->refcnt = 1;

      release(&bcache.buckets[index].lock);
      acquiresleep(&bf->lock);
      return bf;
    }
    bf = bf->next;
  }

  //find empty buf in the whole bcache, move the empty buf
  //to index buckets
  for (int i = 0; i < NBUCKETS; i++) {
    if(i == index)
      continue;

    acquire(&bcache.buckets[i].lock);
    bf = bcache.buckets[index].buf;

    struct buf* cur = bcache.buckets[i].buf;
    struct buf* pre = 0;

    while (cur) {
      if (cur->refcnt == 0) {
        cur->dev = dev;
        cur->blockno = blockno;
        cur->valid = 0;
        cur->refcnt = 1;

        if(pre){                //cur is not the head of the bucket
          pre->next = cur->next;
          cur->next = bf;
          bcache.buckets[index].buf = cur;
        }else{
          pre = cur->next;
          bcache.buckets[i].buf = pre;
          cur->next = bf;
          bcache.buckets[index].buf = cur;
        }

        release(&bcache.buckets[index].lock);
        release(&bcache.buckets[i].lock);
        acquiresleep(&cur->lock);
        return cur;
      }

      pre = cur;
      cur = cur->next;
    }
    release(&bcache.buckets[i].lock);
}

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");
  int index = b->blockno % NBUCKETS;
  acquire(&bcache.buckets[index].lock);
  b->refcnt--;
  release(&bcache.buckets[index].lock);
  releasesleep(&b->lock);
}

void bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
