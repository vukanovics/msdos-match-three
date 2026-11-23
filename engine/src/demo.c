#include "engine/demo.h"

#include <stdio.h>
#include <string.h>

bool demo_open(demo *this, demo_mode mode, const char *path, char *error,
               size_t error_size) {
  memset(this, 0, sizeof(*this));
  this->mode = mode;
  this->path = path;
  this->mismatch_frame = 0xffffffffu;

  if (mode != DEMO_PLAY) {
    return true;
  }

  FILE *file = fopen(path, "r");
  if (!file) {
    snprintf(error, error_size, "cannot open %s", path);
    return false;
  }

  char line[128];
  bool in_hashes = false;
  uint32_t hashes_read = 0;

  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#') {
      continue;
    }

    if (in_hashes) {
      char *at = line;
      while (hashes_read < this->frames) {
        unsigned value;
        int used = 0;
        if (sscanf(at, "%x%n", &value, &used) != 1) {
          break;
        }
        this->hash[hashes_read++] = (uint32_t)value;
        at += used;
      }
      continue;
    }

    unsigned first;
    unsigned second;
    if (sscanf(line, "frames %u", &first) == 1) {
      if (first > DEMO_MAX_FRAMES) {
        fclose(file);
        snprintf(error, error_size, "%s has %u frames, more than %d", path,
                 first, DEMO_MAX_FRAMES);
        return false;
      }
      this->frames = first;
    } else if (sscanf(line, "key %u %u", &first, &second) == 2) {
      if (this->keys >= DEMO_MAX_KEYS) {
        fclose(file);
        snprintf(error, error_size, "%s has more than %d keys", path,
                 DEMO_MAX_KEYS);
        return false;
      }
      this->key[this->keys].frame = first;
      this->key[this->keys].key = (uint8_t)second;
      this->keys++;
    } else if (!strncmp(line, "hashes", 6)) {
      in_hashes = true;
    }
  }

  fclose(file);

  if (hashes_read != this->frames) {
    snprintf(error, error_size, "%s has %lu of %lu checksums", path,
             (unsigned long)hashes_read, (unsigned long)this->frames);
    return false;
  }
  return true;
}

void demo_record_key(demo *this, uint32_t frame, uint8_t key) {
  if (this->keys >= DEMO_MAX_KEYS) {
    this->truncated = true;
    return;
  }
  this->key[this->keys].frame = frame;
  this->key[this->keys].key = key;
  this->keys++;
}

void demo_record_hash(demo *this, uint32_t frame, uint32_t hash) {
  if (frame >= DEMO_MAX_FRAMES) {
    this->truncated = true;
    return;
  }
  this->hash[frame] = hash;
  this->frames = frame + 1;
}

int demo_keys_at(demo *this, uint32_t frame, uint8_t *keys, int max) {
  int count = 0;
  while (this->next_key < this->keys &&
         this->key[this->next_key].frame == frame) {
    if (count < max) {
      keys[count++] = this->key[this->next_key].key;
    }
    this->next_key++;
  }
  return count;
}

void demo_check_hash(demo *this, uint32_t frame, uint32_t hash) {
  if (frame >= this->frames) {
    return;
  }
  if (this->hash[frame] != hash && this->mismatch_frame == 0xffffffffu) {
    this->mismatch_frame = frame;
  }
}

void demo_close(demo *this, char *summary, size_t summary_size) {
  if (this->mode == DEMO_PLAY) {
    if (this->mismatch_frame == 0xffffffffu) {
      snprintf(summary, summary_size,
               "demo %s: %lu frames, %lu keys, every frame matched",
               this->path, (unsigned long)this->frames,
               (unsigned long)this->keys);
    } else {
      snprintf(summary, summary_size,
               "demo %s: %lu frames, %lu keys, FIRST MISMATCH at frame %lu",
               this->path, (unsigned long)this->frames,
               (unsigned long)this->keys,
               (unsigned long)this->mismatch_frame);
    }
    return;
  }

  if (this->mode != DEMO_RECORD) {
    summary[0] = '\0';
    return;
  }

  FILE *file = fopen(this->path, "w");
  if (!file) {
    snprintf(summary, summary_size, "cannot write %s", this->path);
    return;
  }

  fprintf(file, "# engine demo: keys by frame, then the checksum of each frame\n");
  fprintf(file, "frames %lu\n", (unsigned long)this->frames);
  for (uint32_t i = 0; i < this->keys; i++) {
    fprintf(file, "key %lu %u\n", (unsigned long)this->key[i].frame,
            (unsigned)this->key[i].key);
  }
  fprintf(file, "hashes\n");
  for (uint32_t i = 0; i < this->frames; i++) {
    fprintf(file, "%08lx%c", (unsigned long)this->hash[i],
            (i % 8 == 7) ? '\n' : ' ');
  }
  if (this->frames % 8 != 0) {
    fprintf(file, "\n");
  }
  fclose(file);

  snprintf(summary, summary_size, "recorded %lu frames, %lu keys -> %s%s",
           (unsigned long)this->frames, (unsigned long)this->keys, this->path,
           this->truncated ? " (truncated)" : "");
}
