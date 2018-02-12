//
// Created by richard on 11/02/18.
//

#ifndef EZNETWORK_IOMANIP_H
#define EZNETWORK_IOMANIP_H

#include <iostream>
#include <arpa/inet.h>

namespace eznet
{
    /**
     * @brief A template type safe wrapper around htons and htonl
     * @tparam T the type of the argument
     * @param v the value of the argument
     * @return the transformed value
     */
    template<typename T>
    T hton(T v) {
        if constexpr(std::is_same<T, char>::value) {
            return v;
        }
        if constexpr(std::is_same<T, uint16_t>::value) {
            return ::htons(v);
        } else if constexpr (std::is_same<T, uint32_t>::value) {
            return ::htonl(v);
        } else {
            static_assert(std::is_same<std::decay_t<T>, uint32_t>::value, "No implementation of hton for type.");
        }
    }


    /**
     * @brief A template type safe wrapper around ntohs and ntohl
     * @tparam T the type of the argument
     * @param v the value of the argument
     * @return the transformed value
     */
    template<typename T>
    T ntoh(T v) {
        if constexpr(std::is_same<T, char>::value) {
            return v;
        } else if constexpr(std::is_same<T, uint16_t>::value) {
            return ::ntohs(v);
        } else if constexpr (std::is_same<T, uint32_t>::value) {
            return ::ntohl(v);
        } else {
            static_assert(std::is_same<T, uint32_t>::value, "No implementation of ntoh for type.");
        }
    }


    /**
     * @brief A function to transorm a range from host to network format.
     * @tparam C The iterator type
     * @param first the beginning of the range
     * @param last the end of the range
     */
    template<class C>
    void Host2Net(C first, C last) {
        for (auto i = first; i != last; ++i) {
            auto v = hton(*i);
            *i = v;
        }
    }

    /**
     * @brief A function to transorm a range from network to host format.
     * @tparam C The iterator type
     * @param first the beginning of the range
     * @param last the end of the range
     */
    template<class C>
    void Net2Host(C first, C last) {
        for (auto i = first; i != last; ++i) {
            auto v = ntoh(*i);
            *i = v;
        }
    }

#if 0
    int getPacketDepth() {
        static int packetDepth = std::ios_base::xalloc();
        return packetDepth;
    }


    int getPutLiteral() {
        static int putLiteral = std::ios_base::xalloc();
        return putLiteral;
    }


    std::ostream& net_put_num_bin(std::ostream& os) { os.iword(getPutLiteral()) = 1; return os; }
    std::ostream& net_put_num_str(std::ostream& os) { os.iword(getPutLiteral()) = 0; return os; }

    template <typename T>
    struct eznet_put_num : std::num_put<char> {
        iter_type do_put(iter_type s, std::ios_base& f, char_type fill, T v) const {
            if (f.iword(getPutLiteral())) {
                T t = hton<T>(v);
                f.write
            } else {
                return std::num_put<char>::do_put(s, f, fill, v);
            }
        }
    };
#endif

    struct txval_policy
    {
        static char STX;
        static char ETX;
        static char SO;
    };

    char txval_policy::STX = 0x02;
    char txval_policy::ETX = 0x03;
    char txval_policy::SO = 0x0E;

    template <typename T>
    struct txval
    {
        union value_union
        {
            T value;
            char buf[sizeof(T)];
        };

        T tVal;

        txval() = delete;
        explicit txval(T t) : tVal{t} {}

        void put(std::ostream &os, const string &str) const {
            os.put(txval_policy::STX);
            for (auto c: str) {
                if (c == txval_policy::STX || c == txval_policy::ETX || c == txval_policy::SO) {
                    os.put(txval_policy::SO);
                }
                os.put(c);
            }
            os.put(txval_policy::ETX);
        }

        std::ostream& doXmit(std::ostream &os) const {
            if constexpr(std::is_same<T, std::string>::value) {
                put(os, tVal);
            } else if constexpr (std::is_same<T,const char *>::value) {
                put(os, std::string(tVal));
            } else {
                value_union d{};
                d.value = hton(tVal);
                os.write(d.buf, sizeof(T));
            }
            return os;
        }
    };

    template <typename T>
    std::ostream& operator<<(std::ostream &os, const txval<T> &x) {
        return x.doXmit(os);
    }

    template <class IIT>
    struct txval_range
    {
        IIT _first, _last;
        txval_range() = delete;
        txval_range(IIT &&first, IIT &&last) : _first(first), _last(last) {}
        std::ostream& doXmit(std::ostream &os) {
            while (_first != _last) {
                txval x{*_first};
                x.doXmit(os);
                ++_first;
            }
            return os;
        }
    };

    template <class IIT>
    std::ostream& operator<<(std::ostream &os, eznet::txval_range<IIT>&& x) {
        return x.doXmit(os);
    }

    template <typename T>
    struct rxval
    {
        union value_union {
            ~value_union(){}
            T value;
            char buf[sizeof(T)];
        };

        value_union d;
        T &tRef;

        rxval() = delete;
        ~rxval(){}
        explicit rxval(T &t) : tRef{t}, d{} {}
        std::istream& doRecv(std::istream &is) {
            if constexpr(std::is_same<T, std::string>::value) {
                if (is.get() != txval_policy::STX)
                    throw logic_error("rxval(std::string&) data does not start with STX");

                int c{};
                bool escape{};
                tRef.clear();
                while ((c = is.get()) != txval_policy::ETX) {
                    escape = false;

                    if (c == txval_policy::SO)
                        c = is.get();

                    if (is.eof())
                        throw logic_error("EOF during rxval(std::string&)");

                    tRef.push_back(c);
                }
            } else {
                is.read(d.buf, sizeof(T));
                tRef = ntoh(d.value);
            }
            return is;
        }
    };

    template <typename T>
    std::istream& operator>>(std::istream &is, eznet::rxval<T>&& r) {
        return r.doRecv(is);
    }

    template <class OIT>
    struct rxval_range
    {
        OIT _first, _last;
        rxval_range() = delete;
        rxval_range(OIT &&first, OIT &&last) : _first(first), _last(last) {}
        std::istream& doRecv(std::istream &is) {
            while (_first != _last) {
                rxval r{*_first};
                r.doRecv(is);
                ++_first;
            }
            return is;
        }
    };

    template <class OIT>
    std::istream& operator>>(std::istream &is, eznet::rxval_range<OIT>&& r) {
        return r.doRecv(is);
    }

}

#endif //EZNETWORK_IOMANIP_H
