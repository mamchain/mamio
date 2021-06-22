## What is minemon

* In the blockchain field, miners represent the main workers who maintain operations, and also represent profit-seekers. When there is no profit in the ecology, they will immediately leave. The project is named Minemon, which is a combination of Miner and Mammon, which can be abbreviated as MAM. And at the same time, MAM is also an incentive output on the Minemon blockchain.
* Mammon is used to describe material wealth or greed in the New Testament, just like the profit-seeking and greed of miners. Minemon is a blockchain-based humanity test that aims to keep miners and greedy Mammon in a decentralized mechanism to get a balanced state which aims to keep miners and greedy Mammon in a decentralized mechanism to get a balanced state, using mathematical theories and program codes to solve the impact of miners on the blockchain ecology, so as to realize the sustainable development of the Minemon project and increase the value of incentive output.

## Source installation

* Ubuntu16.04/18.04

### Required tools and libraries
```
sudo apt install -y g++ libboost-all-dev cmake openssl libreadline-dev pkg-config libncurses5-dev autoconf

# ubuntu16.04
sudo apt install -y libssl-dev
sudo apt remove cmake
wget https://github.com/Kitware/CMake/releases/download/v3.18.0/cmake-3.18.0-Linux-x86_64.sh
sh cmake-3.18.0-Linux-x86_64.sh

# ubuntu18.04
sudo apt install -y cmake libssl1.0-dev libprotobuf-dev protobuf-compiler

# Compile libsodium (version >= 1.0.18)
  wget https://github.com/jedisct1/libsodium/releases/download/1.0.18-RELEASE/libsodium-1.0.18.tar.gz --no-check-certificate
  tar -zxvf libsodium-1.0.18.tar.gz
  cd libsodium-1.0.18
  ./configure
  make -j8 && make check
  sudo make install


# Compile protobuf
  wget https://github.com/protocolbuffers/protobuf/releases/download/v3.11.3/protobuf-cpp-3.11.3.tar.gz
  tar -xzvf protobuf-cpp-3.11.3.tar.gz
  cd protobuf-3.11.3
  ./configure --prefix=/usr
  make -j8
  sudo make install

```

### Get source code
```
git clone https://github.com/mamchain/mamio.git
```

### Ubuntu Build
```shell
cd mamio
./INSTALL.sh
minemon -help
```

## Repo Guidelines

* Developers work in their own forks, then submit pull requests when they think their feature or bug fix is ready.
* If it is a simple/trivial/non-controversial change, then one of the development team members simply pulls it.
* If it is a more complicated or potentially controversial change, then the change may be discussed in the pull request.
* The patch will be accepted if there is broad consensus that it is a good thing. Developers should expect to rework and resubmit patches if they don't match the project's coding conventions or are controversial.
* From time to time a pull request will become outdated. If this occurs, and the pull is no longer automatically mergeable; a comment on the pull will be used to issue a warning of closure.  Pull requests closed in this manner will have their corresponding issue labeled 'stagnant'.
