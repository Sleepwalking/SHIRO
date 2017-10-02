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
#include <stdio.h>
#include <stdlib.h>
#include "external/cJSON/cJSON.h"
#include "external/liblrhsmm/common.h"
#include "external/liblrhsmm/serial.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cli-common.h"

static void print_usage() {
  fprintf(stderr,
    "shiro-mkhsmm\n"
    "  -c modeldef-file\n"
    "  -h (print usage)\n");
  exit(1);
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;
  cJSON* modeldef = NULL;
  while((c = getopt(argc, argv, "c:h")) != -1) {
    char* jsonstr = NULL;
    switch(c) {
    case 'c':
      jsonstr = readall(optarg);
      if(jsonstr == NULL) {
        fprintf(stderr, "Error: cannot open %s.\n", optarg);
        return 1;
      }
      modeldef = cJSON_Parse(jsonstr);
      if(modeldef == NULL) {
        fprintf(stderr, "Error: failed to parse %s.\n", optarg);
        return 1;
      }
      free(jsonstr);
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }
  if(modeldef == NULL) {
    fprintf(stderr, "Error: model definition file is not specified.\n");
    return 1;
  }

  int ndurstate = 0;
  cJSON* j_ndurstate = cJSON_GetObjectItem(modeldef, "ndurstate");
  checkvar(ndurstate);
  ndurstate = j_ndurstate -> valueint;

  cJSON* j_streamdef = cJSON_GetObjectItem(modeldef, "streamdef");
  checkvar(streamdef);
  int nstream = cJSON_GetArraySize(j_streamdef);

  lrh_model* hsmm = lrh_create_empty_model(nstream, ndurstate);
  for(int l = 0; l < nstream; l ++) {
    cJSON* j_streamdef_l = cJSON_GetArrayItem(j_streamdef, l);
    cJSON* j_nstate = cJSON_GetObjectItem(j_streamdef_l, "nstate");
    checkvar(nstate);
    cJSON* j_ndim = cJSON_GetObjectItem(j_streamdef_l, "ndim");
    checkvar(ndim);
    int nmix = 1;
    cJSON* j_nmix = cJSON_GetObjectItem(j_streamdef_l, "nmix");
    if(j_nmix != NULL)
      nmix = j_nmix -> valueint;
    cJSON* j_weight = cJSON_GetObjectItem(j_streamdef_l, "weight");
    hsmm -> streams[l] = lrh_create_stream(j_nstate -> valueint, nmix,
      j_ndim -> valueint);
    if(j_weight != NULL)
      hsmm -> streams[l] -> weight = j_weight -> valuedouble;
  }
  for(int i = 0; i < ndurstate; i ++)
    hsmm -> durations[i] = lrh_create_duration();

  cJSON* j_dur_attr = cJSON_GetObjectItem(modeldef, "dur_attr");
  if(j_dur_attr != NULL) {
    int ndur_attr = cJSON_GetArraySize(j_dur_attr);
    for(int i = 0; i < ndur_attr; i ++) {
      cJSON* iattr = cJSON_GetArrayItem(j_dur_attr, i);
      cJSON* j_index = cJSON_GetObjectItem(iattr, "index");
      checkvar(index);
      if(j_index -> valueint >= ndurstate) {
        fprintf(stderr, "Error: invalid index %d for durational state.\n",
          j_index -> valueint);
        return 1;
      }
      int di = j_index -> valueint;
      cJSON* j_floor = cJSON_GetObjectItem(iattr, "floor");
      cJSON* j_ceil  = cJSON_GetObjectItem(iattr, "ceil");
      if(j_floor != NULL) hsmm -> durations[di] -> floor = j_floor -> valueint;
      if(j_ceil  != NULL) hsmm -> durations[di] -> ceil  = j_ceil  -> valueint;
    }
  }

  cmp_ctx_t cmpobj;
  cmp_init(& cmpobj, stdout, file_reader, file_writer);
  lrh_write_model(& cmpobj, hsmm);

  lrh_delete_model(hsmm);

  cJSON_Delete(modeldef);
  return 0;
}
