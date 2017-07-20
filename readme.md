SHIRO
===

Phoneme-to-Speech Alignment Toolkit based on [liblrhsmm](https://github.com/Sleepwalking/liblrhsmm)

Proudly crafted in C and Lua. Licensed under GPLv3.

Introduction
---

SHIRO is a set of tools based on HSMM (Hidden Semi-Markov Model), for aligning phoneme transcription with speech recordings, as well as training phoneme-to-speech alignment models.

Gathering hours of speech data aligned with phoneme transcription is, in most approaches to this date, an important prerequisite to training speech recognizers and synthesizers. Typically this task is automated by an operation called forced alignment using hidden Markov models and in particular, the HTK software bundle has become the standard baseline method for both speech recognition and alignment since mid 90s.

SHIRO presents a lightweight alternative to HTK under a more permissive license. It is like a stripped-down version that only does phoneme-to-speech alignment, but equipped with LRHSMM and written from scratch in a few thousand lines of rock-solid C code (plus a bit of Lua).

### A little bit of history

SHIRO is a sister project of liblrhsmm whose first version was developed over summer back in 2015. SHIRO was initially part of liblrhsmm and later it was merged into Moresampler. Before turned into a toolkit, SHIRO supported flat-start training only, which was why it got the name SHIRO (meaning "white" in Japanese).

### Prerequisites

To work with SHIRO you don't need to know how to derive Baum-Welch and go through the messy log(sum(exp)), but it's better to have some basic concepts about HMM, e.g. knowing what are the "states" - I'm not talking about California or Illinois. Reading through some lecture slides on HMM should suffice.

Components
---

SHIRO consists of the following tools,

| Tool | Description | Input(s) | Output(s) |
| --- | --- | --- | --- |
| `shiro-mkhsmm` | model creation tool | model config. | model |
| `shiro-init` | model initialization tool | model, segmentation | model |
| `shiro-rest` | model re-estimation (a.k.a. training) tool | model, segmentation | model |
| `shiro-align` | aligner (using a trained model) | model, segmentation | segmentation (updated) |
| `shiro-wav2raw` | utility for converting `.wav` files into float binary blobs | `.wav` file | `.raw` file |
| `shiro-xxcc` | a simple cepstral coefficients extractor | `.raw` file | parameter file |
| `shiro-fextr.lua` | a feature extractor wrapper | directory | parameter files |
| `shiro-mkpm.lua` | utility for phonemap creation | phoneset | phonemap |
| `shiro-pm2md.lua` | utility for creating model definition from phonemap | phonemap | model def. |
| `shiro-mkseg.lua` | utility for creating segmentation file from `.csv` table | `.csv` file | segmentation |
| `shiro-seg2lab.lua` | utility for converting segmentation file into Audacity label | segmentation | Audacity label files |

Run them with `-h` option for the usage.

Building
---

It is not yet tested whether SHIRO compiles and runs on Windows (but Windows support is planned).

[`ciglet`](https://github.com/Sleepwalking/ciglet) and [`liblrhsmm`](https://github.com/Sleepwalking/liblrhsmm) are the only library dependencies. You also need lua (version 5.1 or above) or luajit. No 3rd party lua library (besides those included in `external/` already) is needed.

* `cd` into `ciglet`, run `make single-file`. This creates `ciglet.h` and `ciglet.c` under `ciglet/single-file/`. Copy and rename this directory to `shiro/external/ciglet`.
* Put `liblrhsmm` under `shiro/external/` and run `make` from `shiro/external/liblrhsmm/`.
* Finally run `make` from `shiro/`.

For your information, the directory structure should look like

* `shiro/external/`
    * `ciglet/`
        * `ciglet.h`
        * `ciglet.c`
    * `liblrhsmm/`
        * a bunch of `.c` and `.h`
        * `Makefile`, `LICENSE`, `readme.md`, etc.
        * `external/`, `test/`, `build/`
    * `cJSON/`
    * `dkjson.lua`, `getopt.lua`, etc.

Getting Started
---

The following sections include examples based on CMU Arctic speech database.

### Create model and (Arpabet) phoneme definitions for American English

The entire framework is in fact language-oblivious (because the mapping between phoneme and features is data-driven).

That being said, to use SHIRO on any language of your choice, simply replace `arpabet-phoneset.csv` by another list of phonemes.

```bash
lua shiro-mkpm.lua examples/arpabet-phoneset.csv \
  -s 3 -S 3 > phonemap.json
lua shiro-pm2md.lua phonemap.json \
  -d 12 > modeldef.json
```

### Align phonemes and speech using a trained model

First step: feature extraction. Input waves are downsampled to 16000 Hz sample rate and 12-order MFCC with first and second-order delta features is extracted.

```bash
lua shiro-fextr.lua index.csv \
  -d "../cmu_us_bdl_arctic/orig/" \
  -x ./extractors/extractor-xxcc-mfcc12-da-16k -r 16000
```

Second step: create a dummy segmentation from the index file.
```bash
lua shiro-mkseg.lua index.csv \
  -m phonemap.json \
  -d "../cmu_us_bdl_arctic/orig/" \
  -e .param -n 36 -L sil -R sil > unaligned.json
```

Third step: since the search space for HSMM is an order of magnitude larger than HMM, it's more efficient to start from a HMM-based forced alignment, then refine the alignment using HSMM in a pruned search space. When running HSMM training, SHIRO applies such pruning by default.
```bash
./shiro-align \
  -m trained-model.hsmm \
  -s unaligned.json \
  -g > initial-alignment.json
./shiro-align \
  -m trained-model.hsmm \
  -s initial-alignment.json> refined-alignment.json
```

Final step: convert the refined segmentation into label files.
```bash
lua shiro-seg2lab.lua refined-alignment.json -t 0.005
```

`.txt` label files will be created under `../cmu_us_bdl_arctic/orig/`.

### Train a model given speech and phoneme transcription

(Assuming feature extraction has been done.)

First step: create an empty model.

```bash
./shiro-mkhsmm -c modeldef.json > empty.hsmm
```

Second step: initialize the model (flat start initialization scheme).

```bash
lua shiro-mkseg.lua index.csv \
  -m phonemap.json \
  -d "../cmu_us_bdl_arctic/orig/" \
  -e .param -n 36 -L sil -R sil > unaligned-segmentation.json
./shiro-init \
  -m empty.hsmm \
  -s unaligned-segmentation.json \
  -FT > flat.hsmm
```

Third step: bootstrap/pre-train using the HMM training algorithm and update the alignment accordingly.

```bash
./shiro-rest \
  -m flat.hsmm \
  -s unaligned-segmentation.json \
  -n 5 -g > markovian.hsmm
./shiro-align \
  -m markovian.hsmm \
  -s unaligned-segmentation.json \
  -g > markovian-segmentation.json
```

Final step: train the model using the HSMM training algorithm.
```bash
./shiro-rest \
  -m markovian.hsmm \
  -s markovian-segmentation.json \
  -n 5 > trained.hsmm
```

### Using SPTK in place of shiro-xxcc

SHIRO's feature files are binary-compatible with the float blob generated from SPTK, which allows the user to experiment with a plethora of feature types that `shiro-xxcc` do not support. An example of extracting MFCC with SPTK is given in `extractors/extractor-sptk-mfcc12-da-16k.lua`,
```lua
return function (try_excute, path, rawfile)
  local mfccfile = path .. ".mfcc"
  local paramfile = path .. ".param"
  try_execute("frame -l 512 -p 80 \"" .. rawfile .. "\" | " ..
    "mfcc -l 512 -m 12 -s 16 > \"" .. mfccfile .. "\"")
  try_execute("delta -l 12 -d -0.5 0 0.5 -d 0.25 0 -0.5 0 0.25 \"" ..
    mfccfile .. "\" > \"" .. paramfile .. "\"")
end
```

Any Lua file that takes the `rawfile` and outputs a `.param` file will work.

**Note**: parameters generated from `shiro-xxcc` are not guaranteed to match the result from SPTK even under the same configuration.
