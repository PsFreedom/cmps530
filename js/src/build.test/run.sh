#!/bin/bash

MY_OUTFILE="out.log"

./js17 test.js | tee $MY_OUTFILE

