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
//int **grid;

int *grid, *Q;
int W;
int *pac_low, *pac_hi;

#define GRID(i, j) grid[(size_t)((i)+1) * W + ((j) + 1)]

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
      switch (GRID(i, j)) {
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

static void stabilize(void) {
  const int n = N, w = W;
  const int sym = n % 2, h = n / 2;
  const int rows = sym ? h + 1 : n;   
  int *restrict q = Q + 2;            
  int changed;
  do {
    changed = 0;
    for (int i = 0; i < rows; ++i) {
      int *restrict up = grid + (size_t)i * w + 1;   
      int *restrict cur = up + w;                 
      int *restrict dn = cur + w;                 
      const int lo = pac_low[i], hi = pac_hi[i];
      int any = 0;
      for (int j = lo; j <= hi; ++j) {            
        int v = cur[j] >> 2;
        q[j] = v;
        cur[j] &= 3;
        any |= v;
      }
      if (!any) continue;                         
      changed = 1;
      const int dmul = (sym && i == h - 1) ? 2 : 1;
      const int dgo = !(sym && i == h);
      for (int j = lo; j <= hi; ++j) up[j] += q[j];
      if (dgo)
        for (int j = lo; j <= hi; ++j) dn[j] += dmul * q[j];
      for (int j = lo - 1; j <= hi + 1; ++j) cur[j] += q[j - 1] + q[j + 1];
      for (int j = lo; j <= hi; ++j) q[j] = 0;
    }
  } while (changed);
  if (sym)
    for (int i = h + 1; i < n; ++i)
      memcpy(&GRID(i, 0), &GRID(n - 1 - i, 0), n * sizeof(int));
}

void find_identity() {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      GRID(i, j) = 6;
    }
  }
  stabilize();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      GRID(i, j) = 6 - GRID(i, j);
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
  
  W = N + 2;
  grid = (int *)calloc((size_t)(N + 2) * W, sizeof(int));
  Q = (int *)calloc((size_t)W + 4, sizeof(int));

  compute_boundary();

  find_identity();

  render("identity.png");
  free(grid);
  free(Q);
  free(pac_low);
  free(pac_hi);
  return EXIT_SUCCESS;
}
