/*
 * Author: Tommaso Cucinotta <tommaso.cucinotta@santannapisa.it>
 * Copyright(c) 2022
 *
 * License: see LICENSE file
 */

#define __USE_GNU
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <atomic>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>

extern "C" {
#include "ticket_mutex.h"
}

#if defined(__arm__) || defined(__aarch64__)
#   define cpu_relax() asm volatile ("yield" ::: "memory");
#elif defined(__i386__) || defined(__amd64__) || defined(__x86_64__)
#   define cpu_relax() asm volatile ("pause" ::: "memory");
#else
#  define cpu_relax() asm volatile ("nop" ::: "memory");
#endif

int num = 100000;
int cpu_id0 = 0;
int cpu_id1 = 1;

typedef enum {
  SYNC_CONDVAR,
  SYNC_PIPE,
  SYNC_TKT_LOCK,
  SYNC_ATOMIC_WAIT,
  SYNC_FUTEX,
  SYNC_ATOMIC
} sync_method_t;

sync_method_t sync_method = SYNC_CONDVAR;

sync_method_t sync_method_from_str(const char *str) {
  if (strcmp(str, "condvar") == 0)
    return SYNC_CONDVAR;
  else if (strcmp(str, "pipe") == 0)
    return SYNC_PIPE;
  else if (strcmp(str, "tkt-lock") == 0)
    return SYNC_TKT_LOCK;
  else if (strcmp(str, "atomic-wait") == 0)
    return SYNC_ATOMIC_WAIT;
  else if (strcmp(str, "futex") == 0)
    return SYNC_FUTEX;
  else if (strcmp(str, "atomic") == 0)
    return SYNC_ATOMIC;
  else {
    fprintf(stderr, "Unsupported sync-method: %s\n", str);
    exit(1);
  }
}

volatile int exiting = 0;
std::atomic_int atom_a, atom_b;
int p1[2], p2[2];
ticket_mutex_t tlock;
volatile int valid = 0;
volatile char glob_ch;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
char m_ch = '-';

static inline void cpu_noop() {
  if (cpu_id0 == cpu_id1)
    sched_yield();
  else
    cpu_relax();
}

static int futex(int *uaddr, int futex_op, int val,
                 const struct timespec *timeout, int *uaddr2, int val3) {
  return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

static void futex_wait(std::atomic_int *ptr, int val) {
  int err;
  err = futex((int*)ptr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
  if (err < 0 && errno != EAGAIN) {
    fprintf(stderr, "futex(WAIT) failed (%d): %s\n", errno, strerror(errno));
    exit(1);
  }
}

static void futex_notify_one(std::atomic_int *ptr) {
  int err;
  err = futex((int*)ptr, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
  if (err < 0) {
    fprintf(stderr, "futex(WAKE) failed (%d): %s\n", errno, strerror(errno));
    exit(1);
  }
}

void put_ch(char ch) {
  pthread_mutex_lock(&mtx);
  while (m_ch != '-') {
    if (exiting)
      return;
    pthread_cond_wait(&cv, &mtx);
  }
  m_ch = ch;
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mtx);
}

char pop_ch() {
  pthread_mutex_lock(&mtx);
  while (m_ch == '-') {
    if (exiting)
      return '-';
    pthread_cond_wait(&cv, &mtx);
  }
  char ch = m_ch;
  m_ch = '-';
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mtx);
  return ch;
}

void *child_main(void *arg) {
  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  CPU_SET(cpu_id1, &cpus);
  assert(pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus) == 0);

  while (!exiting) {
    char loc_ch __attribute__((unused));
    switch (sync_method) {
    case SYNC_CONDVAR:
      loc_ch = pop_ch();
      put_ch(loc_ch);
      break;
    case SYNC_PIPE:
      char ch;
      int rv __attribute__((unused));
      rv = read(p1[0], &ch, 1);
      rv = write(p2[1], &ch, 1);
        ;
      break;
    case SYNC_TKT_LOCK:
      ticket_mutex_lock(&tlock);
      while (!valid && !exiting) {
	ticket_mutex_unlock(&tlock);
	cpu_noop();
	ticket_mutex_lock(&tlock);
      }
      loc_ch = glob_ch;
      valid = 0;
      ticket_mutex_unlock(&tlock);
      break;
    case SYNC_ATOMIC_WAIT:
      while (atom_a == 0 && !exiting)
        atom_a.wait(0);
      atom_a = 0;
      atom_b = 1;
      atom_b.notify_one();
      break;
    case SYNC_FUTEX:
      while (atom_a == 0 && !exiting)
        futex_wait(&atom_a, 0);
      atom_a = 0;
      atom_b = 1;
      futex_notify_one(&atom_b);
      break;
    case SYNC_ATOMIC:
      while (atom_a == 0 && !exiting)
        cpu_noop();
      atom_a = 0;
      atom_b = 1;
      break;
    default:
      fprintf(stderr, "Unsupported sync method: %d\n", sync_method);
      exit(1);
    }
  }
  return 0;
}

void father_main() {
  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  CPU_SET(cpu_id0, &cpus);
  assert(pthread_setaffinity_np(pthread_self(), sizeof(cpus), &cpus) == 0);

  int i;
  struct timespec t1, t2;

  clock_gettime(CLOCK_MONOTONIC, &t1);

  for (i = 0; i < num; i++) {
    char ch = 'a';
    switch (sync_method) {
    case SYNC_CONDVAR:
      put_ch('a');
      char loc_ch __attribute__((unused));
      loc_ch = pop_ch();
      break;
    case SYNC_PIPE:
      int rv __attribute__((unused));
      rv = write(p1[1], &ch, 1);
      rv = read(p2[0], &ch, 1);
      break;
    case SYNC_TKT_LOCK:
      ticket_mutex_lock(&tlock);
      while (valid && !exiting) {
        ticket_mutex_unlock(&tlock);
        cpu_noop();
        ticket_mutex_lock(&tlock);
      }
      glob_ch = 'a';
      valid = 1;
      ticket_mutex_unlock(&tlock);
      break;
    case SYNC_ATOMIC_WAIT:
      atom_a = 1;
      atom_a.notify_one();
      while (atom_b == 0 && !exiting)
        atom_b.wait(0);
      atom_b = 0;
      break;
    case SYNC_FUTEX:
      atom_a = 1;
      futex_notify_one(&atom_a);
      while (atom_b == 0 && !exiting)
        futex_wait(&atom_b, 0);
      atom_b = 0;
      break;
    case SYNC_ATOMIC:
      atom_a = 1;
      futex_notify_one(&atom_a);
      while (atom_b == 0 && !exiting)
        cpu_noop();
      atom_b = 0;
      break;
    default:
      fprintf(stderr, "Unsupported sync method: %d\n", sync_method);
      exit(1);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &t2);
  unsigned long dt = (t2.tv_sec - t1.tv_sec)*1000000 + (t2.tv_nsec - t1.tv_nsec) / 1000;
  printf("%g\n", ((double)dt) / num);
}

void sighandler(int) {
}

int main(int argc, const char *argv[]) {
  argc--;  argv++;
  while (argc > 0) {
    if (strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0) {
      printf("Usage: pingpong [-c core_id,core_id] [-r repeats] [-s|--sync condvar|pipe|tkt-lock|atomic-wait|futex|atomic]\n");
      exit(0);
    } else if (strcmp(*argv, "-c") == 0) {
      argc--;  argv++;
      assert(argc > 0);
      assert(sscanf(*argv, "%d,%d", &cpu_id0, &cpu_id1) == 2);
    } else if (strcmp(*argv, "-r") == 0) {
      argc--;  argv++;
      assert(argc > 0);
      assert(sscanf(*argv, "%d", &num) == 1);
    } else if (strcmp(*argv, "-s") == 0 || strcmp(*argv, "--sync") == 0) {
      argc--;  argv++;
      assert(argc > 0);
      sync_method = sync_method_from_str(*argv);
    } else {
      printf("Unknown option: %s\n", *argv);
      exit(-1);
    }
    argc--;  argv++;
  }

  signal(SIGQUIT, sighandler);

  assert(pipe(p1) == 0);
  assert(pipe(p2) == 0);

  ticket_mutex_init(&tlock);

  pthread_t child;

  assert(pthread_create(&child, NULL, child_main, NULL) == 0);
  father_main();

  exiting = 1;

  exit(0);
}
