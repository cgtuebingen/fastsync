#!/bin/bash

rm -Rf test
mkdir test
cd test
dd if=/dev/urandom bs=1M count=100 of=filein
ln -s filein linkin
mkdir dirin
mkdir dirfilledin
mkdir dirfilledin/dir
touch dirfilledin/test1
dd if=/dev/urandom bs=1K count=1 of=dirfilledin/test2
dd if=/dev/urandom bs=1M count=1 of=dirfilledin/test3

