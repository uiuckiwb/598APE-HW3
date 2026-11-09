#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static void die(const char *s) {
  fprintf(stderr, "%s\n", s);
  exit(1);
}

int N = 801;
int **grid;
int *pac_low, *pac_hi;

static void compute_boundary(void) {
	pac_low = (int*)malloc(sizeof(int) * N);
	pac_hi  = (int*)malloc(sizeof(int) * N);
	int r = N/2;
	for (int i = 0; i < N; i++) {
		pac_low[i] = 0;
		pac_hi[i] = -1;
		for (int j = 0; j <= MAX(i, N-1-i); j++) {
			if ((i - r) * (i - r) + (j - r) * (j - r) <= r * r) {
				if (pac_hi[i] < 0) {
					pac_low[i] = j;
				}
				pac_hi[i] = j;
			}
		}
	}
}

// you can also use this function to debug print your sandpile
static void render(char *filename) {
  int chan = 3;
  unsigned char *pixels = (unsigned char *)malloc(N * N * chan * sizeof(char));
  int stride = N * chan;
  if (!pixels)
    die("malloc failed for PNG pixel buffer");
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      unsigned char r = 0, g = 0, b = 0;
      switch (grid[i][j]) {
      case 0: // obsidian
        r = 15;
        g = 15;
        b = 25;
        break;
      case 1: // electric cyan
        r = 0;
        g = 210;
        b = 255;
        break;
      case 2: // vibrant magenta
        r = 230;
        g = 30;
        b = 130;
        break;
      case 3: // bright gold
        r = 255;
        g = 200;
        b = 0;
        break;
      default: // white
        r = 0;
        g = 0;
        b = 0;
      }

      int pid = (i * N + j) * chan;
      pixels[pid] = r;
      pixels[pid + 1] = g;
      pixels[pid + 2] = b;
    }
  }
  if (stbi_write_png(filename, N, N, chan, pixels, stride)) {
    printf("Successfully saved sandpile identity to: %s\n", filename);
  } else {
    die("Error: stb_image_write failed to save png file\n");
  }
  free(pixels);
}

static void stabilize() {
  int changed = 0;
  if ( N % 2 == 0 ) {
  do {
    changed = 0;
    for (int i = 0; i < N; ++i) {
      for (int j = pac_low[i]; j <= pac_hi[i]; ++j) {
          if (grid[i][j] < 4)
            continue;
          changed = 1; // topple it
          grid[i][j] -= 4;
          if (i > 0)
            grid[i - 1][j]++;
          if (j > 0)
            grid[i][j - 1]++;
          if (i < N - 1)
            grid[i + 1][j]++;
          if (j < N - 1)
            grid[i][j + 1]++;
      }
    }
  } while (changed);
  }
  else {
  int h = N / 2;
  do {
    changed = 0;
    for (int i = 0; i <= h; ++i) {
      for (int j = pac_low[i]; j <= pac_hi[i]; ++j) {
          if (grid[i][j] < 4)
            continue;
          changed = 1; // topple it
          grid[i][j] -= 4;
          if (i > 0)
            grid[i - 1][j]++;
          if (j > 0)
            grid[i][j - 1]++;
          if (i < h)
            grid[i + 1][j] += (i == h - 1) ? 2 : 1; //add 2 from the other row if its the last one in the half
          if (j < N - 1)
            grid[i][j + 1]++;
      }
    }
  } while (changed);

  for (int i = h + 1; i < N; i++) {
    memcpy(grid[i], grid[N - 1 - i], N * sizeof(int));
  }
  }
}

void find_identity() {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      grid[i][j] = 6;
    }
  }
  stabilize();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      grid[i][j] = 6 - grid[i][j];
    }
  }
  stabilize();
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 2) {
    fprintf(stderr, "Usage: %s N\n", argv[0]);
    return EXIT_FAILURE;
  }
  char *end;
  uintmax_t parsed = strtoumax(argv[1], &end, 10);
  if (errno || argv[1][0] < '0' || argv[1][0] > '9' || *end || !parsed ||
      parsed > INT32_MAX)
    die("grid size must be a positive int");
  N = (int)parsed;

  compute_boundary();
  
  grid = (int **)malloc(N * sizeof(int *));
  for (int i = 0; i < N; ++i) {
    grid[i] = (int *)malloc(N * sizeof(int));
  }

  find_identity();

  render("identity.png");
  for (int i = 0; i < N; ++i) {
    free(grid[i]);
  }
  free(grid);
  return EXIT_SUCCESS;
}
