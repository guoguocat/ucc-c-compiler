// RUN: %ucc %s -c
// RUN: %ucc -Xprint %s | grep -F 'typeof(typeof(size_t)) a'

typedef long unsigned size_t;
typeof(typeof(size_t)) a;
