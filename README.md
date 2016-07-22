# lockfree_rcu_hashtable
A C++ wrapper around URCU hashtable

This class implements std::map syntax (most of it), allowing for drop-in replacament.

see http://liburcu.org/ for the original C library and documentation

tested on : Ubuntu 15.10, with g++ 5.2.1

Usage
=====
See the class comment for examples and constraints

Installation
============

Install liburcu: liburcu is available on most major Linux distributions.

for the unit tests:
sudo apt-get install libgtest0 libgtest-dev libpoco-dev

Running the unit tests
======
make
./test


