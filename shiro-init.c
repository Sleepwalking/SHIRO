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
#include "external/cJSON/cJSON.h"
#include "external/liblrhsmm/common.h"
#include "external/liblrhsmm/estimate.h"
#include "external/liblrhsmm/serial.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cli-common.h"

static void print_usage() {
  fprintf(stderr,
    "shiro-init\n"
    "  -m model-file\n"
    "  -s segmentation-file\n"
    "  -v variance-floor\n"
    "  -F (flat start, i.e., starting from uniform state duration)\n"
    "  -T (globally tied flat start)\n"
    "  -h (print usage)\n");
  exit(1);
}

static void assign_uniform_state_duration(lrh_seg* s, lrh_observ* o) {
  FP_TYPE dur = (FP_TYPE)o -> nt / s -> nseg;
  for(int i = 0; i < s -> nseg; i ++)
    s -> time[i] = floor((i + 1.0) * dur);
}

static void collapse_output_states(lrh_seg* s) {
  for(int l = 0; l < s -> nstream; l ++)
    for(int i = 0; i < s -> nseg; i ++)
      s -> outstate[l][i] = 0;
}

static void duplicate_zeroth_state(lrh_model* h) {
  for(int l = 0; l < h -> nstream; l ++)
    for(int i = 1; i < h -> streams[l] -> ngmm; i ++) {
      lrh_delete_gmm(h -> streams[l] -> gmms[i]);
      h -> streams[l] -> gmms[i] = lrh_gmm_copy(h -> streams[l] -> gmms[0]);
    }
}

static void set_variance_floor(lrh_model* h, FP_TYPE ratio) {
  for(int l = 0; l < h -> nstream; l ++)
    for(int i = 1; i < h -> streams[l] -> ngmm; i ++) {
      lrh_gmm* g = h -> streams[l] -> gmms[i];
      for(int k = 0; k < g -> nmix; k ++)
        for(int j = 0; j < g -> ndim; j ++)
          lrh_gmmvf(g, k, j) = lrh_gmmv(g, k, j) * ratio;
    }
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;
  cJSON* j_segm = NULL;
  lrh_model* hsmm = NULL;

  int opt_flatstart = 0;
  int opt_globltied = 0;
  FP_TYPE opt_variancefloor = 0.1;
  while((c = getopt(argc, argv, "m:s:v:FTh")) != -1) {
    char* jsonstr = NULL;
    switch(c) {
    case 'm':
      hsmm = load_model(optarg);
      if(hsmm == NULL) {
        fprintf(stderr, "Error: failed to load model from %s\n", optarg);
        return 1;
      }
    break;
    case 's':
      jsonstr = readall(optarg);
      if(jsonstr == NULL) {
        fprintf(stderr, "Error: cannot open %s.\n", optarg);
        return 1;
      }
      j_segm = cJSON_Parse(jsonstr);
      if(j_segm == NULL) {
        fprintf(stderr, "Error: failed to parse %s.\n", optarg);
        return 1;
      }
      free(jsonstr);
    break;
    case 'v':
      opt_variancefloor = atof(optarg);
    break;
    case 'F':
      opt_flatstart = 1;
    break;
    case 'T':
      opt_globltied = 1;
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }
  if(j_segm == NULL) {
    fprintf(stderr, "Error: segmentation file is not specified.\n");
    return 1;
  }
  if(hsmm == NULL) {
    fprintf(stderr, "Error: model file is not specified.\n");
    return 1;
  }

  cJSON* j_file_list = cJSON_GetObjectItem(j_segm, "file_list");
  checkvar(file_list);
  int nfile = cJSON_GetArraySize(j_file_list);

  lrh_model_stat* hstat = lrh_model_stat_from_model(hsmm);

  for(int f = 0; f < nfile; f ++) {
    cJSON* j_file_list_f = cJSON_GetArrayItem(j_file_list, f);
    cJSON* j_filename = cJSON_GetObjectItem(j_file_list_f, "filename");
    checkvar(filename);
    cJSON* j_states = cJSON_GetObjectItem(j_file_list_f, "states");
    checkvar(states);

    lrh_observ* o = load_observ_from_float(j_filename -> valuestring, hsmm);
    lrh_seg* s = load_seg_from_json(j_states, hsmm -> nstream);
    for(int i = 0; i < s -> nseg; i ++)
      if(s -> time[i] > o -> nt)
        s -> time[i] = o -> nt;

    if(opt_flatstart)
      assign_uniform_state_duration(s, o);
    if(opt_globltied)
      collapse_output_states(s); // all stats go into gmms[0]

    lrh_collect_init(hstat, o, s);

    lrh_delete_seg(s);
    lrh_delete_observ(o);
  }

  lrh_model_update(hsmm, hstat, 0);

  if(opt_globltied)
    duplicate_zeroth_state(hsmm);
  set_variance_floor(hsmm, opt_variancefloor);

  cmp_ctx_t cmpobj;
  cmp_init(& cmpobj, stdout, file_reader, file_writer);
  lrh_write_model(& cmpobj, hsmm);

  cJSON_Delete(j_segm);
  lrh_delete_model_stat(hstat);
  lrh_delete_model(hsmm);
  return 0;
}
