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
#include "external/liblrhsmm/inference.h"
#include "external/liblrhsmm/serial.h"

#include "cli-common.h"

static void print_usage() {
  fprintf(stderr,
    "shiro-align\n"
    "  -m model-file\n"
    "  -s segmentation-file\n"
    "  -g (use geometric duration distribution)\n"
    "  -h (print usage)\n");
  exit(1);
}

extern char* optarg;
int main(int argc, char** argv) {
  int c;
  cJSON* j_segm = NULL;
  lrh_model* hsmm = NULL;

  int opt_geodur = 0;
  while((c = getopt(argc, argv, "m:s:gh")) != -1) {
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
    case 'g':
      opt_geodur = 1;
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }
  if(j_segm == NULL) {
    fprintf(stderr, "Error: cegmentation file is not specified.\n");
    return 1;
  }
  if(hsmm == NULL) {
    fprintf(stderr, "Error: model file is not specified.\n");
    return 1;
  }

  cJSON* j_file_list = cJSON_GetObjectItem(j_segm, "file_list");
  checkvar(file_list);
  int nfile = cJSON_GetArraySize(j_file_list);

  lrh_model_precompute(hsmm);
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

    FP_TYPE* outp = NULL;
    int* realign = NULL;
    if(opt_geodur) {
      outp = lrh_sample_outputprob_lg_full(hsmm, o, s);
      realign = lrh_viterbi_geometric(hsmm, s, outp, o -> nt, NULL);
    } else {
      outp = lrh_sample_outputprob_lg(hsmm, o, s);
      realign = lrh_viterbi(hsmm, s, outp, o -> nt, NULL);
    }
    for(int i = 0; i < s -> nseg; i ++)
      s -> time[i] = realign[i];
    free(outp); free(realign);

    j_states = json_from_seg(s, j_states);
    cJSON_ReplaceItemInObject(j_file_list_f, "states", j_states);

    lrh_delete_seg(s);
    lrh_delete_observ(o);
  }

  char* jsonstr = cJSON_Print(j_segm);
  printf("%s\n", jsonstr);
  free(jsonstr);

  cJSON_Delete(j_segm);
  lrh_delete_model(hsmm);
  return 0;
}
