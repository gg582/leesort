#!/bin/sh
# Fetch the third-party sorts used by `make compare`, pinned to the commits that were benchmarked.
set -e
cd "$(dirname "$0")/.."
mkdir -p third_party
fetch() { # repo dir commit
    if [ ! -d "third_party/$2" ]; then
        git clone -q "https://github.com/$1" "third_party/$2"
    fi
    git -C "third_party/$2" checkout -q "$3"
}
fetch orlp/pdqsort        pdqsort       b1ef26a55cdb60d236a5cb199c4234c704f46726
fetch skarupke/ska_sort   ska_sort      2c14d4bf7b667032ed28a481065f721385e33849
fetch intel/x86-simd-sort x86-simd-sort 7521047beb89928cf7a1b56fec55be584c9b0916
fetch swenson/sort        sort          24f5b8b13810ad130109c7b56daf8e99ab0fe1b8
