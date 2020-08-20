#!/bin/sh

set -e
for i in board1 board2 board3 board4 board5 board6 board7 board8
do
    git checkout $i
    git pull
    git cherry-pick $@
    git push
done

echo This script has left you in branch $i.
