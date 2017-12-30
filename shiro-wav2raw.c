/*
  SHIRO
  ===
  Copyright (c) 2017 Kanru Hua. All rights reserved.

  This file is part of SHIRO.

  SHIRO is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  SHIRO is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SHIRO.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "external/ciglet/ciglet.h"

char* mystrdup(const char *str) {
  int n = strlen(str) + 1;
  char* ret = malloc(n);
  strcpy(ret, str);
  return ret;
}

char* get_output_path(const char* input, const char* ext) {
  int n1 = strlen(input);
  int n2 = strlen(ext);
  char* ret = malloc(n1 + n2 + 1);
  strcpy(ret, input);
  char* rext = ret + n1 - 1;
  while(rext != ret && *rext != '.') {
    if(*rext == '/' || *rext == '\\' || rext == ret + 1) {
      strcpy(ret + n1, ext);
      return ret;
    }
    rext --;
  }
  strcpy(rext, ext);
  return ret;
}

static void write_float_data(const char* path, FP_TYPE* x, int nx) {
  FILE* fout = fopen(path, "wb");
  if(fout == NULL) {
    fprintf(stderr, "Error: cannot write to %s\n", path);
    exit(1);
  }
  float* xfloat = calloc(nx, sizeof(float));
  for(int i = 0; i < nx; i ++) xfloat[i] = x[i];
  fwrite(xfloat, 4, nx, fout);
  free(xfloat);
  fclose(fout);
}

static void print_usage() {
  fprintf(stderr,
    "shiro-wav2raw path-to-wav-file\n"
    "  -e extension of the output\n"
    "  -r sample rate of the output\n"
    "  -d dithering noise level\n"
    "  -N (normalize)\n"
    "  -h (print usage)\n");
  exit(1);
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;

  char* input_wav = NULL;
  int opt_normalize = 0;
  char* opt_extension = mystrdup(".raw");
  int opt_fs = 0;
  float dithering = 0;
  while((c = getopt(argc, argv, "e:r:d:Nh")) != -1) {
    switch(c) {
    case 'e':
      free(opt_extension);
      opt_extension = mystrdup(optarg);
    break;
    case 'r':
      opt_fs = atoi(optarg);
      if(opt_fs <= 0) {
        fprintf(stderr, "Error: invalid sample rate.\n");
        exit(1);
      }
    break;
    case 'd':
      dithering = atof(optarg);
    break;
    case 'N':
      opt_normalize = 1;
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }

  if(optind >= argc) {
    fprintf(stderr, "Error: missing argument path-to-wav-file.\n");
    exit(1);
  }
  input_wav = mystrdup(argv[optind]);

  char* output_raw = get_output_path(input_wav, opt_extension);

  int nx, fs, nbit;
  FP_TYPE* x = wavread(input_wav, & fs, & nbit, & nx);
  if(x == NULL) {
    fprintf(stderr, "Error: cannot open %s.\n", input_wav);
    exit(1);
  }

  if(opt_normalize) {
    FP_TYPE maxampl = 0;
    for(int i = 0; i < nx; i ++)
      if(fabs(x[i]) > maxampl)
        maxampl = fabs(x[i]);
    for(int i = 0; i < nx; i ++)
      x[i] /= maxampl;
  }

  if(dithering > 0) {
    for(int i = 0; i < nx; i ++)
      x[i] += randu() * dithering;
  }
  
  if(opt_fs > 0 && opt_fs != fs) { // needs resampling
    FP_TYPE ratio = (FP_TYPE)opt_fs / fs;
    int ny = 0;
    FP_TYPE* y = rresample(x, nx, ratio, & ny);
    write_float_data(output_raw, y, ny);
    free(y);
  } else {
    write_float_data(output_raw, x, nx);
  }

  free(x);
  free(opt_extension);
  free(input_wav);
  free(output_raw);
  return 0;
}
