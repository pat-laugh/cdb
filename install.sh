#!/usr/bin/env bash

run_apt_get_install='sudo apt-get install g++ make libboost-all-dev'
echo "\
The binaries g++ and make, and the C++ Boost libraries Filesystem
and System are required. The following command can install them:

$run_apt_get_install
"
read -p 'Install them now (the command above will be executed) [y/N]? ' a
if [ "$a" = y ]; then
    $run_apt_get_install
fi
unset a run_apt_get_install

mkdir -p ~/bin

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
    local line name path
    name=$1
    cp $name .$name
    sed -i "s|$name|$HOME/bin/.$name|" .$name
    path="$HOME/bin/.$name.sh"
    mv .$name "$path"
    line=". $path"
    grep -Fx "$line" ~/.bashrc 2>&1 >/dev/null
    if [ $? != 0 ]; then
        echo "$line" >>~/.bashrc
    fi
}
cd front-end/bash-completion
cdb_mv cdb-bc
cd ../src
mv cdb cdb-back
cdb_mv cdb-back
mv cdb-back cdb
unset cdb_mv

cd "$cur_dir"
. ~/.bashrc
echo "\
To start using cdb, source the ~/.bashrc file, or open a new terminal.

. ~/.bashrc
"
