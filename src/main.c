#include <graphics/drawing.h>
#include <graphics/color.h>
#include <graphics/text.h>
#include <graphics/init.h>
#include <graphics/lcdc.h>
#include <sh4a/input/keypad.h>
#include <syscalls/syscalls.h>
#include <stdio.h>
#include <string.h>

#define SW 528
#define SH 320
#define APPID "RMSCN"
#define BLOCK_BYTES 0x00040000UL
#define BLOCK_WORDS (BLOCK_BYTES / 4UL)
#define BLOCK_COUNT 32U
#define TARGET_P1_BASE 0x8c800000UL
#define TARGET_P2_BASE 0xac800000UL
#define BACKUP_P2_BASE 0xac4be400UL

enum scan_result {
  SCAN_PASS = 0,
  SCAN_PATTERN1_FAIL = 1,
  SCAN_PATTERN2_FAIL = 2,
  SCAN_RESTORE_FAIL = 3
};

static void release_key(int key) {
  while (get_key_state(key)) keypad_read();
}

static void clear_screen(void) {
  set_pen(create_rgb16(0, 0, 0));
  draw_rect(0, 0, SW, SH);
}

static void text_line(int y, const char *s, unsigned short color) {
  set_pen(color);
  render_text(12, y, (char *)s);
}

static void draw_menu(unsigned int block) {
  char s[96];
  unsigned long p1 = TARGET_P1_BASE + (unsigned long)block * BLOCK_BYTES;
  unsigned long p2 = TARGET_P2_BASE + (unsigned long)block * BLOCK_BYTES;
  clear_screen();
  text_line(10, "EX-WORD RAM SCANNER / DP7", create_rgb16(0, 255, 255));
  text_line(45, "Automatic scan from the selected 256 KiB block", create_rgb16(255, 255, 0));
  sprintf(s, "Block: %02u / %02u", block, BLOCK_COUNT - 1U);
  text_line(85, s, create_rgb16(255, 255, 255));
  sprintf(s, "P1 logical: %08x - %08x", (unsigned int)p1,
          (unsigned int)(p1 + BLOCK_BYTES - 1UL));
  text_line(115, s, create_rgb16(255, 255, 255));
  sprintf(s, "P2 access : %08x - %08x", (unsigned int)p2,
          (unsigned int)(p2 + BLOCK_BYTES - 1UL));
  text_line(145, s, create_rgb16(180, 180, 180));
  text_line(190, "WARNING: an unsafe block may freeze the unit.", create_rgb16(255, 80, 80));
  text_line(220, "Use battery reset only if input stops.", create_rgb16(255, 80, 80));
  text_line(290, "UP/DOWN: start block   ENTER: continue", create_rgb16(0, 255, 0));
  lcdc_copy_vram();
}

static void draw_confirm(unsigned int block) {
  char s[96];
  unsigned long p1 = TARGET_P1_BASE + (unsigned long)block * BLOCK_BYTES;
  clear_screen();
  text_line(10, "CONFIRM RAM TEST", create_rgb16(255, 255, 0));
  sprintf(s, "Block %02u at %08x", block, (unsigned int)p1);
  text_line(65, s, create_rgb16(255, 255, 255));
  text_line(105, "Original data is copied to a separate arena.", create_rgb16(255, 255, 255));
  text_line(135, "Scanning continues through block 31.", create_rgb16(255, 255, 255));
  text_line(180, "Do not power off while the test is running.", create_rgb16(255, 80, 80));
  text_line(290, "ENTER: start   BACK: cancel", create_rgb16(0, 255, 0));
  lcdc_copy_vram();
}

static void draw_progress(unsigned int block, const char *phase) {
  char s[64];
  clear_screen();
  text_line(10, "RAM TEST IN PROGRESS", create_rgb16(255, 255, 0));
  sprintf(s, "Block %02u", block);
  text_line(80, s, create_rgb16(255, 255, 255));
  text_line(125, phase, create_rgb16(255, 255, 255));
  text_line(190, "Do not power off.", create_rgb16(255, 80, 80));
  lcdc_copy_vram();
}

static void make_path(char *path, const char *name) {
  unsigned long drive;
  char id[10];
  sys_dict_info(&drive, id);
  sprintf(path, "\\\\drv0\\%s\\_USER\\%s", id, name);
}

static int write_all(int fd, const char *buf, int length) {
  int done = 0;
  while (done < length) {
    int n = sys_write(fd, buf + done, (unsigned long)(length - done));
    if (n <= 0) return 0;
    done += n;
  }
  return 1;
}

static int append_log(const char *line) {
  char path[64];
  int fd, size, ok;
  make_path(path, "RAMSCAN.TSV");
  fd = sys_open(path, FILE_WR);
  if (fd < 0) {
    if (sys_create(path, 1) < 0) return 0;
    fd = sys_open(path, FILE_WR);
    if (fd < 0) return 0;
  }
  size = sys_get_filesize(fd);
  if (size < 0 || sys_seek(fd, size, 0) < 0) {
    sys_close(fd);
    return 0;
  }
  ok = write_all(fd, line, (int)strlen(line));
  sys_close(fd);
  return ok;
}

static unsigned int first_unpassed_block(void) {
  static char logbuf[8193];
  unsigned char passed[BLOCK_COUNT];
  char path[64];
  int fd, size, got, i;
  memset(passed, 0, sizeof(passed));
  make_path(path, "RAMSCAN.TSV");
  fd = sys_open(path, FILE_RD);
  if (fd < 0) return 0;
  size = sys_get_filesize(fd);
  if (size < 0) {
    sys_close(fd);
    return 0;
  }
  if (size > 8192) size = 8192;
  got = sys_read(fd, logbuf, (unsigned long)size);
  sys_close(fd);
  if (got <= 0) return 0;
  logbuf[got] = 0;
  for (i = 0; i + 11 < got; ++i) {
    if (!memcmp(logbuf + i, "PASS\tblock=", 11)) {
      unsigned int n = 0;
      int j = i + 11;
      while (j < got && logbuf[j] >= '0' && logbuf[j] <= '9') {
        n = n * 10U + (unsigned int)(logbuf[j] - '0');
        ++j;
      }
      if (n < BLOCK_COUNT) passed[n] = 1;
    }
  }
  for (i = 0; i < (int)BLOCK_COUNT; ++i) if (!passed[i]) return (unsigned int)i;
  return BLOCK_COUNT - 1U;
}

static int inter_block_pause(unsigned int completed, unsigned int next) {
  volatile unsigned char *rtc_sec = (volatile unsigned char *)0xa413fec2;
  unsigned char previous = *rtc_sec;
  unsigned int changes = 0;
  unsigned long guard = 0;
  char s[80];
  clear_screen();
  text_line(10, "BLOCK RESTORED", create_rgb16(0, 255, 0));
  sprintf(s, "Block %02u passed. Next: block %02u", completed, next);
  text_line(90, s, create_rgb16(255, 255, 255));
  text_line(145, "BACK: stop safely before the next block", create_rgb16(255, 255, 0));
  lcdc_copy_vram();
  while (changes < 2U && guard++ < 300000000UL) {
    unsigned char now;
    keypad_read();
    if (get_key_state(KEY_BACK) || get_key_state(KEY_POWER)) return 0;
    now = *rtc_sec;
    if (now != previous) {
      previous = now;
      ++changes;
    }
  }
  return 1;
}

static unsigned long fnv1a(volatile unsigned char *p, unsigned long n) {
  unsigned long h = 2166136261UL;
  unsigned long i;
  for (i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 16777619UL;
  }
  return h;
}

static unsigned long pattern1(unsigned long i, unsigned int block) {
  return 0xa5a50000UL ^ (i * 0x9e3779b9UL) ^ (unsigned long)block;
}

static unsigned long pattern2(unsigned long i, unsigned int block) {
  return 0x5a5affffUL ^ (i * 0x7f4a7c15UL) ^ ((unsigned long)block << 24);
}

static int scan_block(unsigned int block, unsigned long *before_hash,
                      unsigned long *after_hash, unsigned long *bad_offset) {
  volatile unsigned long *target = (volatile unsigned long *)(TARGET_P2_BASE +
                                      (unsigned long)block * BLOCK_BYTES);
  volatile unsigned long *backup = (volatile unsigned long *)BACKUP_P2_BASE;
  unsigned long i;
  int result = SCAN_PASS;

  draw_progress(block, "Backing up original data...");
  for (i = 0; i < BLOCK_WORDS; ++i) backup[i] = target[i];
  *before_hash = fnv1a((volatile unsigned char *)backup, BLOCK_BYTES);

  draw_progress(block, "Writing and verifying pattern 1...");
  for (i = 0; i < BLOCK_WORDS; ++i) target[i] = pattern1(i, block);
  for (i = 0; i < BLOCK_WORDS; ++i) {
    if (target[i] != pattern1(i, block)) {
      *bad_offset = i * 4UL;
      result = SCAN_PATTERN1_FAIL;
      break;
    }
  }

  if (result == SCAN_PASS) {
    draw_progress(block, "Writing and verifying pattern 2...");
    for (i = 0; i < BLOCK_WORDS; ++i) target[i] = pattern2(i, block);
    for (i = 0; i < BLOCK_WORDS; ++i) {
      if (target[i] != pattern2(i, block)) {
        *bad_offset = i * 4UL;
        result = SCAN_PATTERN2_FAIL;
        break;
      }
    }
  }

  draw_progress(block, "Restoring and verifying original data...");
  for (i = 0; i < BLOCK_WORDS; ++i) target[i] = backup[i];
  for (i = 0; i < BLOCK_WORDS; ++i) {
    if (target[i] != backup[i]) {
      *bad_offset = i * 4UL;
      result = SCAN_RESTORE_FAIL;
      break;
    }
  }
  *after_hash = fnv1a((volatile unsigned char *)target, BLOCK_BYTES);
  if (*after_hash != *before_hash) result = SCAN_RESTORE_FAIL;
  return result;
}

static const char *result_name(int result) {
  if (result == SCAN_PASS) return "PASS";
  if (result == SCAN_PATTERN1_FAIL) return "FAIL_PATTERN1";
  if (result == SCAN_PATTERN2_FAIL) return "FAIL_PATTERN2";
  return "FAIL_RESTORE";
}

static void draw_result(unsigned int block, int result, unsigned long before_hash,
                        unsigned long after_hash, unsigned long bad_offset,
                        int log_ok) {
  char s[96];
  unsigned short color = result == SCAN_PASS ? create_rgb16(0, 255, 0) : create_rgb16(255, 0, 0);
  clear_screen();
  text_line(10, "RAM SCAN RESULT", create_rgb16(0, 255, 255));
  sprintf(s, "Block %02u: %s", block, result_name(result));
  text_line(55, s, color);
  sprintf(s, "Before FNV1a: %08x", (unsigned int)before_hash);
  text_line(95, s, create_rgb16(255, 255, 255));
  sprintf(s, "After  FNV1a: %08x", (unsigned int)after_hash);
  text_line(125, s, create_rgb16(255, 255, 255));
  if (result != SCAN_PASS) {
    sprintf(s, "First mismatch offset: %08x", (unsigned int)bad_offset);
    text_line(160, s, create_rgb16(255, 80, 80));
  }
  text_line(205, log_ok ? "Result log saved." : "WARNING: final log save failed.",
            log_ok ? create_rgb16(0, 255, 0) : create_rgb16(255, 0, 0));
  text_line(290, "BACK / POWER: exit", create_rgb16(0, 255, 0));
  lcdc_copy_vram();
}

int main(void *ptr) {
  unsigned int block;
  int confirming = 0;
  if (ptr && *(long *)ptr == 1) return -1;
  graphics_init(SW, SH, (void *)0xAC200000);
  block = first_unpassed_block();
  draw_menu(block);

  for (;;) {
    keypad_read();
    if (get_key_state(KEY_POWER)) return -2;
    if (get_key_state(KEY_BACK)) {
      release_key(KEY_BACK);
      if (confirming) {
        confirming = 0;
        draw_menu(block);
      } else return -2;
    }
    if (!confirming && get_key_state(KEY_UP)) {
      release_key(KEY_UP);
      block = (block + BLOCK_COUNT - 1U) % BLOCK_COUNT;
      draw_menu(block);
    }
    if (!confirming && get_key_state(KEY_DOWN)) {
      release_key(KEY_DOWN);
      block = (block + 1U) % BLOCK_COUNT;
      draw_menu(block);
    }
    if (get_key_state(KEY_ENTER)) {
      char line[192];
      unsigned int current, completed = 0;
      unsigned long p1 = 0, before_hash = 0, after_hash = 0, bad_offset = 0xffffffffUL;
      int result = SCAN_PASS, log_ok = 1, stopped = 0;
      release_key(KEY_ENTER);
      if (!confirming) {
        confirming = 1;
        draw_confirm(block);
        continue;
      }

      for (current = block; current < BLOCK_COUNT; ++current) {
        p1 = TARGET_P1_BASE + (unsigned long)current * BLOCK_BYTES;
        before_hash = after_hash = 0;
        bad_offset = 0xffffffffUL;
        sprintf(line, "BEGIN\tblock=%u\tp1=%08x\tp2=%08x\tsize=%u\r\n",
                current, (unsigned int)p1,
                (unsigned int)(TARGET_P2_BASE + (unsigned long)current * BLOCK_BYTES),
                (unsigned int)BLOCK_BYTES);
        log_ok = append_log(line);
        if (!log_ok) {
          result = SCAN_RESTORE_FAIL;
          break;
        }
        result = scan_block(current, &before_hash, &after_hash, &bad_offset);
        sprintf(line, "%s\tblock=%u\tp1=%08x\tsize=%u\tbefore=%08x\tafter=%08x\tbad_offset=%08x\r\n",
                result_name(result), current, (unsigned int)p1, (unsigned int)BLOCK_BYTES,
                (unsigned int)before_hash, (unsigned int)after_hash, (unsigned int)bad_offset);
        log_ok = append_log(line);
        ++completed;
        if (result != SCAN_PASS || !log_ok) break;
        if (current + 1U < BLOCK_COUNT && !inter_block_pause(current, current + 1U)) {
          stopped = 1;
          break;
        }
      }
      if (stopped) {
        clear_screen();
        text_line(45, "AUTOMATIC SCAN STOPPED SAFELY", create_rgb16(255, 255, 0));
        sprintf(line, "%u block(s) completed in this run.", completed);
        text_line(100, line, create_rgb16(255, 255, 255));
        text_line(290, "BACK / POWER: exit", create_rgb16(0, 255, 0));
        lcdc_copy_vram();
      } else {
        draw_result(current < BLOCK_COUNT ? current : BLOCK_COUNT - 1U,
                    result, before_hash, after_hash, bad_offset, log_ok);
      }
      for (;;) {
        keypad_read();
        if (get_key_state(KEY_POWER) || get_key_state(KEY_BACK)) return -2;
      }
    }
  }
}
