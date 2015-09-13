/*=============================================================================

In sets, all items are automatically sorted with the STL implementation of
std::less<T>. However, this compares pointers only by their address value.
In our project we need them dereferenced so that duplicates can be successfully
avoided.

Author   : Benedikt Hiemenz, Max Kolhagen, Markus Schmidt
=============================================================================*/
#ifndef COMPARISON_H
#define COMPARISON_H

// pointer comparator used for sets w/ pointers
struct pt_cmp
{
    // cannot be const, as it delegates to Signable::getHash
    template<typename T>
    bool operator()(T /*const*/* t1, T /*const*/* t2) /*const*/
    {
        return *t1 < *t2;
    }
};

#endif // COMPARISON_H
