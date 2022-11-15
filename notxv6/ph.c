#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5  //哈希桶数
#define NKEYS 100000

pthread_mutex_t lock[NBUCKET];   //哈希桶的锁

struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];   //哈希表共有5个bucket，每个bucket都是一个由entry组成的链表
//key：由各线程管理的keys[]从随机值映射而来；value：写入的线程的序号
int keys[NKEYS];   //键值队列
int nthread = 1;

double
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void 
insert(int key, int value, struct entry **p, struct entry *n)   //多线程操作哈希表出错
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;  //n为哈希表桶的链表头，多线程情况下，不同线程put时头插的链表头相同，则导致数据丢失
  *p = e;  //链表头更新
}

static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;  //初始化指针指向0（并非为空）
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)  //哈希桶的链表中的线程序号相同
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    pthread_mutex_lock(&lock[i]);
    insert(key, value, &table[i], table[i]);   //多线程情况下不同线程调用insert，此时table[i]代表的bucket链表头
    //相同，此时会发生漏插入键值对的情况，需要用锁去维护（锁的粒度不能太大）
    pthread_mutex_unlock(&lock[i]);
  }

}

static struct entry*
get(int key)
{
  int i = key % NBUCKET;

  //如果put时insert无异常不会出现键值对丢失的情况，则get的时候自然也不会有数据遗漏
  //pthread_mutex_lock(&lock[i]);
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }
  //pthread_mutex_unlock(&lock[i]);
  return e;
}

//哈希表put推入线程创建
static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);    //以单线程举例：keys[100000*0+[0,99999]]（全是随机值），即可能映射到哈希表中任意一个桶
    //双线程：线程0占keys[0,49999]，线程1占keys[50000,99999]
    //索引到哈希表的桶时可能会发生桶bucket的冲突，于是需要给bucket上锁
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }

  for(int i=0;i<NBUCKET;i++)
  {
    pthread_mutex_init(&lock[i],NULL);
  }

  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);
  assert(NKEYS % nthread == 0);
  for (int i = 0; i < NKEYS; i++) {   //随机生成100000个键值对
    keys[i] = random();
  }

  //
  // first the puts，将键值对插入到哈希表中
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
