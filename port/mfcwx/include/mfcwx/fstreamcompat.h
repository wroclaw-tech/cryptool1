#pragma once

// File streams that accept Windows-style paths ("dir\\file.txt") and are constructible
// from const char* by copy-initialization, as the pre-standard <fstream.h> allowed.

#include <fstream>
#include <string>

#include "mfcwx/codepage.h"

namespace std {

class mfcwx_ifstream : public basic_ifstream<char> {
public:
    mfcwx_ifstream() = default;
    mfcwx_ifstream(const char* path, ios_base::openmode mode = ios_base::in)
        : basic_ifstream<char>(::mfcwx::NativePath(path), mode) {}
    mfcwx_ifstream(const string& path, ios_base::openmode mode = ios_base::in)
        : mfcwx_ifstream(path.c_str(), mode) {}
    void open(const char* path, ios_base::openmode mode = ios_base::in) {
        basic_ifstream<char>::open(::mfcwx::NativePath(path), mode);
    }
    void open(const string& path, ios_base::openmode mode = ios_base::in) { open(path.c_str(), mode); }
};

class mfcwx_ofstream : public basic_ofstream<char> {
public:
    mfcwx_ofstream() = default;
    mfcwx_ofstream(const char* path, ios_base::openmode mode = ios_base::out)
        : basic_ofstream<char>(::mfcwx::NativePath(path), mode) {}
    mfcwx_ofstream(const string& path, ios_base::openmode mode = ios_base::out)
        : mfcwx_ofstream(path.c_str(), mode) {}
    void open(const char* path, ios_base::openmode mode = ios_base::out) {
        basic_ofstream<char>::open(::mfcwx::NativePath(path), mode);
    }
    void open(const string& path, ios_base::openmode mode = ios_base::out) { open(path.c_str(), mode); }
};

class mfcwx_fstream : public basic_fstream<char> {
public:
    mfcwx_fstream() = default;
    mfcwx_fstream(const char* path, ios_base::openmode mode = ios_base::in | ios_base::out)
        : basic_fstream<char>(::mfcwx::NativePath(path), mode) {}
    mfcwx_fstream(const string& path, ios_base::openmode mode = ios_base::in | ios_base::out)
        : mfcwx_fstream(path.c_str(), mode) {}
    void open(const char* path, ios_base::openmode mode = ios_base::in | ios_base::out) {
        basic_fstream<char>::open(::mfcwx::NativePath(path), mode);
    }
    void open(const string& path, ios_base::openmode mode = ios_base::in | ios_base::out) {
        open(path.c_str(), mode);
    }
};

} // namespace std

using std::mfcwx_fstream;
using std::mfcwx_ifstream;
using std::mfcwx_ofstream;

#define ifstream mfcwx_ifstream
#define ofstream mfcwx_ofstream
#define fstream mfcwx_fstream
