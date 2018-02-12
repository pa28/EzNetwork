//
// Created by richard on 11/02/18.
//

#include <iostream>
#include <iomanip>
#include "server.h"

#include "iomanip.h"

using namespace std;
using namespace eznet;

/**
 * @brief A simple test of manipulators for transmitting binary data.
 */
int main() {
    /*
     * Set policy values for easier debugging.
     */
    txval_policy::STX = '<';
    txval_policy::ETX = '>';
    txval_policy::SO = '\\';
    txval_policy::US = ',';

    std::cout << "Hello, World!" << std::endl;

    /*
     * Create two arrays of test data. Choose data that maps to ASCII characters for easy debugging
     */
    array<uint16_t, 3> a16{0x4142, 0x4344, 0x4546};
    array<uint32_t, 3> a32{0x41424344, 0x45464748, 0x494a4b4c};

    /*
     * And two arrays to receive data into
     */
    array<uint16_t, 3> r16{};
    array<uint32_t, 3> r32{};

    /*
     * Display the initial values
     */
    cout << "a16 ";
    for (auto a: a16) {
        cout << hex << setw(4) << setfill('0') << a << ' ';
    }
    cout << endl;

    cout << "a32 ";
    for (auto a: a32) {
        cout << hex << setw(4) << setfill('0') << a << ' ';
    }
    cout << endl;

    /*
     * Write the test data to a stringstream.
     */
    stringstream ss;

    ss << txval_range(a16.begin(), a16.end()) << txsep
       << txval_range(a32.begin(), a32.end()) << txsep
       << txval("Hello World!");

    /*
     * Display the data in the string stream.
     */
    cout << "Buffer in network order: " << ss.str() << endl;

    /*
     * Receive the data from the string stream
     */
    string hello;
    ss >> rxval_range(r16.begin(), r16.end()) >> rxsep
       >> rxval_range(r32.begin(), r32.end()) >> rxsep
       >> rxval(hello);

    /*
     * Display the received results.
     */
    cout << "r16 ";
    for (auto a: r16) {
        cout << hex << setw(4) << setfill('0') << a << ' ';
    }
    cout << endl;

    cout << "r32 ";
    for (auto a: r32) {
        cout << hex << setw(4) << setfill('0') << a << ' ';
    }
    cout << endl;

    cout << "String: " << hello << endl;

    return 0;
}

