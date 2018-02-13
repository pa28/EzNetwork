//
// Created by hrbuckl on 2/13/18.
//

#ifndef EZNETWORK_NAME_THAT_TYPE_H
#define EZNETWORK_NAME_THAT_TYPE_H

/**
 * A little boiler plate to have the compiler fail but print the type of parameter passed to name_that_type
 */

template <class T> class that_type;
template <class T> void name_that_type(T& param) {
    that_type<T> tType;
    that_type<decltype(param)> paramType;
}

#endif //EZNETWORK_NAME_THAT_TYPE_H
