#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "readersWriters.h"
#include "err.h"

#define READERS 3
#define WRITERS 2
#define NAP 2
#define BSIZE 32

typedef struct ReadWrite {
    pthread_mutex_t lock;
    pthread_cond_t readers;
    pthread_cond_t writers;
    pthread_cond_t cleaners;
    int rcount, wcount, rwait, wwait;
    int change;
} ReadWrite;

// ReadWrite library;
// char book[BSIZE];
// int working = 1;
int err;

/* Initialize a buffer */

ReadWrite *rw_init() {
    //int err;
    ReadWrite *rw = malloc(sizeof(ReadWrite));
    if (!rw) return NULL;

    if ((err = pthread_mutex_init(&rw->lock, 0)) != 0) {
        syserr(err, "pthread_mutex_init failed");
    }
    if ((err = pthread_cond_init(&rw->readers, 0)) != 0) {
        syserr(err, "pthread_cond_init for readers failed");
    }
    if ((err = pthread_cond_init(&rw->writers, 0)) != 0) {
        syserr(err, "pthread_cond_init for writers failed");
    }
    if ((err = pthread_cond_init(&rw->cleaners, 0)) != 0) {
        syserr(err, "pthread_cond_init for writers failed");
    }
    rw->change = 0;
    rw->rwait = 0;
    rw->rcount = 0;
    rw->wcount = 0;
    rw->wcount = 0;

    return rw;
}

/* Destroy the buffer */

void destroy(struct ReadWrite *rw) {
    //int err;
    if ((err = pthread_cond_destroy(&rw->writers)) != 0) {
        syserr(err, "pthread_cond_destroy for writers failed");
    }
    if ((err = pthread_cond_destroy(&rw->readers)) != 0) {
        syserr(err, "pthread_cond_destroy for readers failed");
    }
    if ((err = pthread_cond_destroy(&rw->cleaners)) != 0) {
        syserr(err, "pthread_cond_destroy for cleaners failed");
    }
    if ((err = pthread_mutex_destroy(&rw->lock))) {
        syserr(err, "pthread_mutex_destroy failed");
    }
}

void mutexLock(pthread_mutex_t lock) {
    //int err;
    if ((err = pthread_mutex_lock(&lock)) != 0) {
        syserr(err, "pthread_mutex_lock failed");
    }
}

void mutexUnlock(pthread_mutex_t lock) {
    //int err;
    if ((err = pthread_mutex_unlock(&lock)) != 0) {
        syserr(err, "pthread_mutex_unlock failed");
    }
}

void signalReaders(pthread_cond_t readers) {
    //int err;
    if ((err = pthread_cond_signal(&readers)) != 0) {
        syserr(err, "pthread_cond_signal failed");
    }
}

void signalWriters(pthread_cond_t writers) {
    //int err;
    if ((err = pthread_cond_signal(&writers)) != 0) {
        syserr(err, "pthread_cond_signal failed");
    }
}

void signalCleaners(pthread_cond_t cleaners) {
    //int err;
    if ((err = pthread_cond_signal(&cleaners)) != 0) {
        syserr(err, "pthread_cond_signal failed");
    }
}

void waitReaders(ReadWrite *rw) {
    //int err;
    if ((err = pthread_cond_wait(&rw->readers, &rw->lock)) != 0) {
        syserr(err, "pthread_cond_wait failed");
    }
}

void waitWriters(ReadWrite *rw) {
    //int err;
    if ((err = pthread_cond_wait(&rw->writers, &rw->lock)) != 0) {
        syserr(err, "pthread_cond_wait failed");
    }
}

void waitCleaners(ReadWrite *rw) {
    //int err;
    if ((err = pthread_cond_wait(&rw->cleaners, &rw->lock)) != 0) {
        syserr(err, "pthread_cond_wait failed");
    }
}

void read_prepare(ReadWrite *rw) {
    mutexLock(rw->lock);
    while (rw->change <= 0 && rw->wcount + rw->wwait > 0) {
        ++rw->rwait;
        // if ((err = pthread_cond_wait(&rw->readers, &rw->lock)) != 0)
        // {
        //   syserr(err, "pthread_cond_wait failed");
        // }
        waitReaders(rw);
        --rw->rwait;
    }
    if (rw->change > 0)
        --rw->change;

    ++rw->rcount;
    if (rw->change > 0)
        signalReaders(rw->readers);

    mutexUnlock(rw->lock);
}

void read_close(ReadWrite *rw) {
    mutexLock(rw->lock);
    --rw->rcount;
    if (rw->rcount == 0) {
        if (rw->wwait > 0) {
            rw->change = -1;
            signalWriters(rw->writers);
        } else {
            signalCleaners(rw->cleaners);
        }
    }
    mutexUnlock(rw->lock);
}

void write_prepare(ReadWrite *rw) {
    //int err;
    mutexLock(rw->lock);

    while (rw->change != -1 && rw->wcount + rw->rcount > 0) {
        ++rw->wwait;
        if ((err = pthread_cond_wait(&rw->writers, &rw->lock)) != 0) {
            syserr(err, "pthread_cond_wait failed");
        }
        //waitWriters(rw);
        --rw->wwait;
    }
    rw->change = 0;
    ++rw->wcount;

    mutexUnlock(rw->lock);
}

void write_close(ReadWrite *rw) {
    mutexLock(rw->lock);
    --rw->wcount;

    if (rw->rwait > 0) {
        rw->change = rw->rwait;
        signalReaders(rw->readers);
    } else if (rw->wwait > 0) {
        rw->change = -1;
        signalWriters(rw->writers);
    } else {
        signalCleaners(rw->cleaners);
    }
    mutexUnlock(rw->lock);
}

void clean_prepare(ReadWrite *rw) {
    mutexLock(rw->lock);
    while (rw->rwait > 0 && rw->wwait > 0 && rw->rcount > 0 && rw->wcount > 0) {
        waitCleaners(rw);
    }
    mutexUnlock(rw->lock);
}
