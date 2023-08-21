# ELE3021_12300_2019038513_Park

# Design

### Multi Indirect

먼저 dinode구조체에 각각 double, triple의 테이블 주소를 담기 위해 addrs배열에 2를 추가해준다. 

이후 bmap함수에서 bn값을 비교해가며 어떤 테이블에 할당할지 결정한다. 이때 기존의 코드 구조를 반복적으로 활용하여 테이블에 파일을 맵핑한다. 또한 각각의 구간에 들어갈 수 있도록 코드가 진행됨에 따라 각 테이블이 수용하는 블록 넘버와 현재 블록 넘버를 비교한다. 

예를 들어 200개의 블록들을 저장한다면 10 + 128 + 62 로 나누어 각각을 다이렉트, 인다이렉트 더블 인다이렉트에 맵핑을 하게 한다.

결론적으로 알맞은 위치의 테이블 구간에 진입하게 된다.

### Symbolic Link

먼저 inode 구조체에 원본 파일의 path를 저장해야 하기 때문에 해당 내용을 추가해준다. 이후에 심볼릭 링크 파일을 생성하는 시스템 콜을 작성한다. 여기서는 기존과 비슷하게 노드를 생성해주고 해당 노드에 존재하는 심볼릭 파일의 path를 저장하는 변수에 기존 파일의 path를 복사해준다. 해당 파일의 메타 데이터를 간략하게 입력 한뒤 끝낸다. 추가적으로 해당 노드의 type 값을 새롭게 정의한 심볼릭 타입으로 넣어준다. 

또한 추가적으로 readi시에 심볼릭 파일이면 심볼릭 파일 자체를 여는 것이 아닌 해당 구조체에 저장되어있는 원본 path에 대한 파일을 열도록 한다.

### Sycn

먼저 기존에는 지속적으로 group flush하였지만 이를 막습니다. 수정 이후에는 사용자가 sync()함수를 호출하거나 버퍼가 꽉 찼을 때에 flush를 하도록 변경합니다. 먼저 sync 시스템 콜을 만들어 사용자가 이를 호출하였을 때 flush 되도록 합니다. 이 함수는  log_write함수와 비슷한 기능을 합니다. 로그에 기록된 데이터들을 실제 디스크에 쓰게 해주는 함수입니다. 두번째로 버퍼가 가득 찬 상황을 인지해야 하기 때문에 log 구조체에 가득 찼는지 아닌지에 대한 상황을 판별할 수 있는 변수를 두고 가득 찼다면 flush하게 해줍니다.

# Implement

### Multi Indirect

```c
#define FSSIZE       2000000  // size of file system in blocks
```

먼저 파일 시스템의 크기를 전체적으로 키워준다.

```c
#define NDIRECT 10 // dinode의 크기는 일정해야 하기 때문에 NDIRECT의 수를 2개 줄여준다
#define NINDIRECT (BSIZE / sizeof(uint))
#define DINDIRECT 128*128
#define TINDIRECT 128*128*128
#define MAXFILE (NDIRECT + NINDIRECT + DINDIRECT + TINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+3];   // Data block addresses + double indirect + triple indirect
};
```

더블, 트리플에 대한 테이블을 추가해야 하기 때문에 addrs의 길이를 2 늘려준다. 이때 크기는 유지해야 하기 때문에 NDIRECT의 크기는 2 줄여준다. 

```c
static uint
bmap(struct inode *ip, uint bn)
{
	...

  bn -= NINDIRECT; // 앞의 부분은 이미 저장되었기 때문에 빼준다.

  if(bn < DINDIRECT) {
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT+1]) == 0) // 만약에 해당하는 더블 테이블이 아직 할당되지 않았다면 할당한다.
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev); // 할당받은 주소를 더블 테이블의 칸에 할당시켜준다. 
    bp = bread(ip->dev, addr); // 블록을 디스크로부터 읽어옴, 블록 포인터 반환
    a = (uint*)bp->data;

    uint double_table = bn/NINDIRECT; // 첫번째 테이블 에서의 인덱스를 찾아간다.

    if((addr = a[double_table]) == 0 ) { // 테이블이 존재하지 않는다면 할당해야함
      a[double_table] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    uint double_entry = bn%NINDIRECT; //data table에서의 인덱스

    if((addr = a[double_entry]) == 0 ) {
      a[double_entry] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    return addr; // 블록이 저장된 주소를 반환
  }

// 트리플도더블과 마찬가지로 알맞은 블록에 넣는다.

  if(bn < TINDIRECT) {
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT+2]) == 0)
      ip->addrs[NDIRECT+2] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    uint triple_table = bn/DINDIRECT;

    if((addr = a[triple_table]) == 0) {
      a[triple_table] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    uint triple_table_table = (bn%DINDIRECT)/NINDIRECT; 
    if((addr = a[triple_table_table]) == 0) {
      a[triple_table_table] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;

    uint triple_entry = bn%NINDIRECT;
    if((addr = a[triple_entry]) == 0) {
      a[triple_entry] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    return addr;
  }

  panic("bmap: out of range");
}
```

넣으려는 블럭 넘버에 따라 알맞은 곳에 맵핑되도록 하는 함수이다. 이때 주의할 점은 단계를 거쳐갈 수록 앞의 테이블에대한 오프셋을 빼주고 다음 단계의 테이블에서 위치를 찾아야 한다는 것이다. 트리플의 경우 첫번째테이블의 인덱스는 BN/DINDIRECT로 구하고 그 다음 테이블에 대해서는 (bn%DINDIRECT)/NINDIRECT 최종적으로는 bn%NINDIRECT에 블럭을 맵핑해준다. 

### Symbolic Link

```c
#define T_SYM 8
```

해당 노드의 타입을 심볼릭이라는 것을 표시하기 위해 새로운 타입을 정의하였다.

```c
int sys_symlink(void) {
  // 심볼릭 링크 시스템콜
  char *new, *old; // 기존의 파일과 새롭게 만들 심볼릭 파일 
  struct file *f;
  struct inode *ip;

// 사용자에게 넘어온 입력값을 받는다.
  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  // 심볼릭 파일을 위한 새로운 파일을 생성한다. 이때 심볼릭 이므로 타입을 T_SYMLINK로 지정한디.
  if((ip = create(new, T_SYM, 0,0))==0) {
    end_op();
    return -1; // 파일 생성 실패시 끝
  }

  // 비어있는 inode를 할당받음
  if((f=filealloc()) == 0) {
    if(f) {
      fileclose(f);
    }
    iunlockput(ip);
    return -1;
  }

  safestrcpy(ip->symbolic_link, old, 30); // 심볼릭 노드의 symbolic_link변수에 원본의 path를 넣어준다
  iunlock(ip);

  f->ip = ip; // 해당 파일에 대한 메타 데이터를 입력해준다.
  f->off = 0;
  f->readable = 1;
  f->writable = 0;

  ilock(ip);
  iunlock(ip);

  return 0;
}
```

사용자가 심볼릭 시스템 콜을 호출한다면 이 함수가 실행된다. 먼저 사용자의 입력을 받는데 첫번째는 기존 파일의 이름이고 두번째는 새롭게 생성할 심볼릭 파일의 이름이다. 이후 심볼릭을 위한 새로운 파일을 create함수를 통해 생성한다. 이때 노드의 타입을 심볼릭 이라는 것을 표시한다. 

다음으로는 filealloc을 통해 비어있는 노드를 할당받고 ip와 연결해준다. 

safestrcpy함수를 통해 기존 파일의 path를 심볼릭 노드 구조체의 symbolic_link에 저장한다.

```c
// in-memory copy of an inode
struct inode {
  ...

  char symbolic_link[DIRSIZ+1]; // 심볼릭일때 진짜 파일의 주소를 저장하는 변수
};
```

inode 구조체에 실제 파일의 path를 저장할 수 있는 배열을 추가한다.

```c
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
...
  if(ip->type == T_SYM) {
		// 심볼릭일 때 실제 파일의 ip를 불러와 교체시켜버린다.
    ip = namei(ip->symbolic_link);
  }

...
}
```

readi 함수를 호출할 때 심볼릭 타입이라면 ip를 실제 파일의 path로 바꿔치기 할 수 있게 하였다. 

sys_open → namei → namex → dirlookup → readi 이 구조로 진행되기 때문에 open시에 수정된 path로 열리게 된다.

### 시스템콜 선언

```c
int             sync(void);
```

```c
extern int sys_symlink(void);
extern int sys_sync(void);

[SYS_symlink] sys_symlink,
[SYS_sync] sys_sync,
```

```c
#define SYS_symlink 23
#define SYS_sync 24
```

```c
int symlink(const char*, const char*);
int sync(void);
```

```c
SYSCALL(symlink)
SYSCALL(sync)
```

# Result

![심볼릭 파일을 생성하였다.](ELE3021_12300_2019038513_Park%20ae270f89927d4156a509a21c5cbef7bc/Untitled.png)

심볼릭 파일을 생성하였다.

# Trobule Shooting

sync를 구상하기는 했지만 구현을 하지 못했습니다. 다음과 같은 시도를 하였으나 부팅 후 sh가 실행되질 않는 문제가 있어 수정 전 버전을 제출하였습니다.

### log.c

```c
struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;

  int is_full; // 로그가 꽉 찼는지 확인하는 변수
};
```

```c
int sys_sync(void) {
  if(log.lh.n != (LOGSIZE-1)) {
    return -1; // 로그의 크기가 정해둔 크기보다 적거나 크면 flush 안함
  }
  int temp = 0;
  temp = log.lh.n; // 몇개의 블럭을 flush하였는지?
  commit();
  return temp;
}
```

```c
void
end_op(void)
{
  if(log.is_full == 1 && do_commit == 1) {
    sys_sync();
    acquire(&log.lock);
    log.committing = 0;
    // log.is_full == 0;
    wakeup(&log);
    release(&log.lock);
  }
// 기존의 커밋 조건을 로그가 꽉 찼는지를 기준으로 하였습니다. 
}
```

```c
void
log_write(struct buf *b)
{
  ...
  if (i == log.lh.n) {
    log.lh.n++;
    if(log.lh.n == (LOGSIZE-1)) {
      log.is_full = 1;
// 로그가 다 차면 다 찼다는 것을 표시해줍니다.
    }
  }
    
  ...
}
```
