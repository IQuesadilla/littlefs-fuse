/*
 * Linux user-space block device wrapper
 *
 * Copyright (c) 2022, the littlefs authors.
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "lfs_fuse_bd.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>
#if !defined(__FreeBSD__)
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#elif defined(__FreeBSD__)
#define BLKSSZGET DIOCGSECTORSIZE
#define BLKGETSIZE DIOCGMEDIASIZE
#include <sys/disk.h>
#endif

struct my_context {
  int fd;
  int *list;
  int cfsize;
};

// Returns the lseek position of the start of the requested block
off_t lfs_fuse_bd_seekpos(const struct lfs_config *cfg, ssize_t block) {
  struct my_context *ctx = cfg->context;

  if (ctx->list[block] < 0) {
    // EXTEND
    lseek(ctx->fd, 0, SEEK_END);
   
    uint8_t buf[cfg->block_size];
    memset(buf, '\0', cfg->block_size);
    write(ctx->fd, buf, cfg->block_size);

    ctx->list[block] = ctx->cfsize;
    ctx->cfsize += 1;
  }

  return ctx->list[block] * cfg->block_size;
}

// Block device wrapper for user-space block devices
int lfs_fuse_bd_create(struct lfs_config *cfg, const char *path) {
  // get sector size
  cfg->block_size = 512; // Must be preset
  cfg->block_count = 2048;  // Allocated as necessary
  cfg->block_cycles = -1;  // Disable wear leveling
  
  struct stat statbuf = {0};
  int err = stat(path, &statbuf);
  if (err == 0) {
    err = unlink(path);
    if (err < 0)
      return -errno;
  }

  err = creat(path, 0644);
  if (err < 0)
    return -errno;

  struct my_context *ctx = malloc(sizeof(struct my_context));
  ctx->list = malloc(sizeof(*ctx->list) * cfg->block_count);
  for (int k = 0; k < cfg->block_count; ++k)
    ctx->list[k] = -1;
  ctx->cfsize = 0;

  int fd = open(path, O_RDWR);
  if (fd < 0) {
      return -errno;
  }
  ctx->fd = fd;
  //lfs_fuse_bd_resize(cfg, 10);

  // setup function pointers
  cfg->read  = lfs_fuse_bd_read;
  cfg->prog  = lfs_fuse_bd_prog;
  cfg->erase = lfs_fuse_bd_erase;
  cfg->sync  = lfs_fuse_bd_sync;

  cfg->context = ctx;

  return 0;
}

void lfs_fuse_bd_destroy(const struct lfs_config *cfg) {
  struct my_context *ctx = cfg->context;
  close(ctx->fd);
  //unlink(path);
  free(ctx);
}

int lfs_fuse_bd_read(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, void *buffer, lfs_size_t size) {
  struct my_context *ctx = cfg->context;
  uint8_t *buffer_ = buffer;

  // check if read is valid
  assert(block < cfg->block_count);

  // go to block
  off_t seekpos = lfs_fuse_bd_seekpos(cfg, block);
  if (seekpos < 0)
    return seekpos;

  off_t err = lseek(ctx->fd, seekpos + (off_t)off, SEEK_SET);
  if (err < 0) {
    return -errno;
  }

  // read block
  while (size > 0) {
    ssize_t res = read(ctx->fd, buffer_, (size_t)size);
    if (res < 0) {
      return -errno;
    }

    buffer_ += res;
    size -= res;
  }

  return 0;
}

int lfs_fuse_bd_prog(const struct lfs_config *cfg, lfs_block_t block,
        lfs_off_t off, const void *buffer, lfs_size_t size) {
  struct my_context *ctx = cfg->context;
  const uint8_t *buffer_ = buffer;

  // check if write is valid
  assert(block < cfg->block_count);

  // go to block
  off_t seekpos = lfs_fuse_bd_seekpos(cfg, block);
  if (seekpos < 0)
    return seekpos;

  off_t err = lseek(ctx->fd, seekpos + (off_t)off, SEEK_SET);
  if (err < 0) {
    return -errno;
  }

  // write block
  while (size > 0) {
    ssize_t res = write(ctx->fd, buffer_, (size_t)size);
    if (res < 0) {
      return -errno;
    }

    buffer_ += res;
    size -= res;
  }

  return 0;
}

int lfs_fuse_bd_erase(const struct lfs_config *cfg, lfs_block_t block) {
  //struct my_context *ctx = cfg->context;
  //ctx->list[block] = -1;
  return 0;
}

int lfs_fuse_bd_sync(const struct lfs_config *cfg) {
  struct my_context *ctx = cfg->context;

  int err = fsync(ctx->fd);
  if (err) {
      return -errno;
  }

  return 0;
}

