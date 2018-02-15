//
// Created by hrbuckl on 2/15/18.
//

#ifndef EZNETWORK_SOCKET_BUFFER_H
#define EZNETWORK_SOCKET_BUFFER_H

#include <iostream>

using namespace std;

namespace async_net {
/**
 * @brief A streambuf which abstracts the socket file descriptor allowing the use of
 * standard iostreams.
 */
    class socket_streambuf : public std::streambuf {
    public:

        typedef char char_type;                             ///< The character type supported
        typedef std::char_traits<char_type> traits_type;    ///< Character traits for char_type
        typedef typename traits_type::int_type int_type;    ///< Integer type

        constexpr static size_t buffer_size = BUFSIZ;       ///< System specified size of buffers
        constexpr static size_t pushback_size = 8;          ///< The minimum number of characters that may be pushed back

        socket_streambuf() = delete;

        /**
         * @brief Create a socket stream buffer interfaced to a Socket object file descriptor.
         * @param sock
         */
        explicit socket_streambuf(int sock) : sockfd(sock), obuf{}, ibuf{} {
            this->setp(obuf, obuf + buffer_size);
            this->setg(ibuf, ibuf + pushback_size, ibuf + pushback_size);
        }

    protected:
        int sockfd;                           ///< The Socket object this buffer interfaces with
        char_type obuf[buffer_size];                    ///< The output stream buffer
        char_type ibuf[buffer_size + pushback_size];    ///< The input stream buffer and pushback space

        /**
         * @brief Flush the contents of the output buffer to the Socket
         * @return 0 on success, -1 on failure.
         */
        int sync() override {
            if (sockfd >= 0) {
                ssize_t n = ::send(sockfd, obuf, pptr() - obuf, 0);

                if (n < 0) {
                    return -1;
                } else if (n == (pptr() - obuf)) {
                    setp(obuf, obuf + buffer_size);
                } else {
                    for (ssize_t cp = n; cp < (buffer_size - n); ++cp) {
                        obuf[cp - n] = obuf[cp];
                    }
                    setp(obuf, obuf + buffer_size);
                    pbump(static_cast<int>(buffer_size - n));
                }
                return 0;
            }
            return -1;
        }

        /**
         * @brief Called when output data won't fit in the output buffer
         * @details The output buffer is flushed to the Socket. The overflow character is placed in the clean
         * buffer.
         * @param c The character that caused the overflow
         * @return the input character
         */
        int_type overflow(int_type c) override {
            if (sync() < 0)
                return traits_type::eof();

            if (traits_type::not_eof(c)) {
                char_type cc = traits_type::to_char_type(c);
                this->xsputn(&cc, 1);
            }

            return c;
        }

        /**
         * @brief Called when there is not enough data in the input buffer to satisfy an request
         * @details More data is read from the underlying Socket. If none is available return EOF,
         * otherwise return the first new character.
         * @return The next available character or EOF
         */
        int_type underflow() override {
            if (sockfd >= 0) {
                ssize_t n = ::recv(sockfd, ibuf + pushback_size, buffer_size - pushback_size, 0);

                if (n < 0) {
                    return traits_type::eof();
                }
                this->setg(ibuf, ibuf + pushback_size, ibuf + pushback_size + n);
                if (n) {
                    return traits_type::to_int_type(*(ibuf + pushback_size));
                }
            }
            return traits_type::eof();
        }


        /**
         * @brief Try to determine how many characters are availalbe in the input stream without blocking.
         * @return >0 the number of characters know to be available, 0 no information, -1 sequence unavailable
         */
        streamsize showmanyc() override {
            ssize_t n = ::recv(sockfd, ibuf + pushback_size, buffer_size - pushback_size, MSG_DONTWAIT);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    n = 0;
                } else {
                    return -1;
                }
            }
            this->setg(ibuf, ibuf + pushback_size, ibuf + pushback_size + n);
            return n;
        }
    };
}

#endif //EZNETWORK_SOCKET_BUFFER_H
