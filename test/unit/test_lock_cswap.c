/*
 * shmem_test_lock_cswap()  test_lock {-v|n}
 *  where:
 *      -v      Enable debugging messages
 *      -n x    Period with which to announce spin count
 *
 * For n loops:
 *   Each PE repeatedly attempts to take a simple spinlock on rank 0 using
 *   cswap, upon success the lock is released and the PE enters a barrier.
 *   On a failed lock attempt, increment local lock_tries counter and repeat.
 */

#include <shmem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define Rfprintf if (shmem_my_pe() == 0) fprintf
#define Rprintf  if (shmem_my_pe() == 0)  printf
#define Vfprintf if (Verbose) fprintf
#define Vprintf  if (Verbose) printf

int Verbose = 0;
int Announce = 0;
int Noise = 500;
int Loops = 40;

long lock;

int
main(int argc, char* argv[])
{
    int c, cloop;
    int my_rank, num_ranks;

    shmem_init();
    my_rank = shmem_my_pe();
    num_ranks = shmem_n_pes();

    if (num_ranks == 1) {
        fprintf(stderr, "ERR - Requires > 1 PEs\n");
        shmem_finalize();
        return 0;
    }

    while ((c = getopt(argc,argv,"n:v")) != -1) {
        switch (c) {
          case 'n':
            Noise = atoi(optarg);
            break;
          case 'v':
            Verbose++;
            Announce = 1;
            break;
          default:
            Rfprintf(stderr,"ERR - unknown -%c ?\n",c);
            shmem_finalize();
            return 1;
        }
    }

    for (cloop=1; cloop <= Loops; cloop++) {
        int got_lock = 0;
        int lock_cnt = 0;
        int tries = 0;

        lock = 0;

        shmem_barrier_all();  /* sync all ranks */

        while (!got_lock) {
            long lockval = shmem_long_cswap(&lock, 0, my_rank+1, 0);

            if (lockval == 0) {
                long unlockval;
                got_lock = 1;

                Vprintf("[%d] locked: lock_cnt(%d) lock(%lx)\n", my_rank, lock_cnt, lock);

                unlockval = shmem_long_cswap(&lock, my_rank+1, 0, 0); /* RACE: PE 1 hangs here */
                if (unlockval != my_rank+1) {
                    printf("[%d] unlock failed, expected %lx got %lx\n", my_rank, (long) my_rank+1, unlockval);
                    shmem_global_exit(1);
                }

                Vprintf("[%d] finished unlock\n", my_rank);
            }
            else {
                tries++;
                if ( Announce && ((tries % (num_ranks*Noise)) == 0) ) {
                    printf("[%d] unsuccessful lock attempts %d lock_cnt %d lock %lx\n",
                            my_rank, tries, lock_cnt, lock);
                }
            }
        }
        shmem_barrier_all();  /* sync all ranks */

        if ((cloop % 10) == 0) {
            if (Announce) {
                Rprintf("%d ranks completed %d loops\n", num_ranks, cloop);
            }
        }
    }

    shmem_barrier_all();  /* sync all ranks */

    Vprintf ("[%d] of %d, Exit\n", my_rank, num_ranks);
    shmem_finalize();
    return 0;
}
