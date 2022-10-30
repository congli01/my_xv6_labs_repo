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
#define NBUCKETS 13   // 哈希桶的数量

struct {
  // 每个哈希桶一个lock
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // 共13个哈希桶
  struct buf hashbucket[NBUCKETS]; 
} bcache;

// 哈希函数，返回哈希值
int hash(int n)
{
  return n % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;

  // 初始化哈希桶
  for (int i = 0; i < NBUCKETS; i++)
  {
    initlock(&bcache.lock[i], "bcache");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  // 将所有缓存块放入哈希桶
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    int hash_num = hash(b->blockno);  // 缓存块号对应的哈希值
    b->next = bcache.hashbucket[hash_num].next;
    b->prev = &bcache.hashbucket[hash_num];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[hash_num].next->prev = b;
    bcache.hashbucket[hash_num].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // 求块号的哈希值，并获取对应的哈希桶的锁
  int hash_num = hash(blockno);
  acquire(&bcache.lock[hash_num]);

  // Is the block already cached?
  for(b = bcache.hashbucket[hash_num].next; b != &bcache.hashbucket[hash_num]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[hash_num]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 首先根据LRU算法在自己的哈希桶中找未被引用的缓存块
  for(b = bcache.hashbucket[hash_num].prev; b != &bcache.hashbucket[hash_num]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hash_num]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 自己的哈希桶中没有空闲的缓存块，窃取其他桶的空闲缓存块
  for (int i = 0; i < NBUCKETS;i++)
  {
    if(i == hash_num) continue;

    acquire(&bcache.lock[i]);
    for(b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev)
    {
      if(b->refcnt == 0)
      {
        // 将找到的缓存块移到原哈希桶链表
        // 移出
        b->prev->next = b->next;
        b->next->prev = b->prev;
        // 移入
        b->prev = &bcache.hashbucket[hash_num];
        b->next = bcache.hashbucket[hash_num].next;
        bcache.hashbucket[hash_num].next->prev = b;
        bcache.hashbucket[hash_num].next = b;

        // 修改该缓存块的属性值
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // 释放锁
        release(&bcache.lock[i]);
        release(&bcache.lock[hash_num]);

        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }
  release(&bcache.lock[hash_num]);

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

  int hash_num = hash(b->blockno);

  acquire(&bcache.lock[hash_num]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[hash_num].next;
    b->prev = &bcache.hashbucket[hash_num];
    bcache.hashbucket[hash_num].next->prev = b;
    bcache.hashbucket[hash_num].next = b;
  }
  
  release(&bcache.lock[hash_num]);
}

void
bpin(struct buf *b) {
  int hash_num = hash(b->blockno);
  acquire(&bcache.lock[hash_num]);
  b->refcnt++;
  release(&bcache.lock[hash_num]);
}

void
bunpin(struct buf *b) {
  int hash_num = hash(b->blockno);
  acquire(&bcache.lock[hash_num]);
  b->refcnt--;
  release(&bcache.lock[hash_num]);
}


