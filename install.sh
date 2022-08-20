#!/usr/bin/env bash

# Build back end
cur_dir=`pwd`
cd back-end/src/execs
make clean
make release
mv cdb-back.out ~/bin/.cdb-back
mv cdb-bc.out ~/bin/.cdb-bc
make clean

# Build and source front end
cd "$cur_dir"
cdb_mv() {
    local name path
    name=$1
    cp $name .$name
    sed -i "s|$name|$HOME/bin/.$name|" .$name
    path="$HOME/bin/.$name.sh"
    mv .$name "$path"
    echo ". $path" >>~/.bashrc
}
cd front-end/bash-completion
cdb_mv cdb-bc
cd ../src
mv cdb cdb-back
cdb_mv cdb-back
mv cdb-back cdb
unset cdb_mv
