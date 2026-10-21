#pragma once
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cwchar>
#include <new>
#include <limits>
#include <utility>
#include <algorithm>
#include "bb_win_compat.h"
#include <io.h>
typedef intptr_t ssize_t;
#define fseeko _fseeki64
#define ftello _ftelli64
#define fileno _fileno
