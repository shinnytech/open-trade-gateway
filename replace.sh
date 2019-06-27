#!/bin/bash

for i in `seq 1 20`;
do
    find . -name "*.cpp" | xargs -L1 perl -i -p0e's/Log([^;]*?)(\(LOG_[^,]+,[^,]+,)\s*"([^"]*?)(?<="|;)([^="]+)=([^%]+?)(?:;|(?="))([^"]*)"/Log$1.WithField("$4", "$5")$2"$3$6"/gm'
    find . -name "*.cpp" | xargs -L1 perl -i -p0e's/Log([^;]*?)(\(LOG_[^,]+,[^,]+,)\s*"([^"]*?)(?<="|;)([^="]+)=%.+?(?:;|(?="))([^"]*)"\s*,\s*(.*?)(?=\s*,|\);)/Log$1.WithField("$4", $6)$2"$3$5"/gm'
    find . -name "*.cpp" | xargs -L1 perl -i -p0e's/Log([^;]*?)\((LOG_[^,]+?)\s*,\s*([^,]+?)\s*,\s*""\s*\);/Log$1.WithField("pack", $3).Write($2);/gm'
    find . -name "*.cpp" | xargs -L1 perl -i -p0e's/\.WithField\("pack",\s*nullptr\s*\)//gm'
done
