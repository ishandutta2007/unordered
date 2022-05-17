// Copyright 2022 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <type_traits>

namespace {
  static int x = 0;
}

namespace map {

  template <class T> struct allocator
  {
    using value_type = T;
    using size_type = std::size_t;

    allocator() = default;

    template <class U> allocator(allocator<U> const&) {}

    template <class U> using rebind = allocator<U>;

    T* allocate(size_type n)
    {
      return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_type) { ::operator delete(p); }

    template <class U, class... Args> void construct(U* p, Args&&... args)
    {
      static_assert(std::is_same<U, std::pair<int const, int> >::value, "");
      ++x;
      new (p) U(std::forward<Args>(args)...);
    }

    template <class U> void destroy(U* p)
    {
      static_assert(std::is_same<U, std::pair<int const, int> >::value, "");
      ++x;
      p->~U();
    }

    bool operator==(allocator const&) const { return true; }
    bool operator!=(allocator const&) const { return false; }
  };
} // namespace map

namespace set {

  template <class T> struct allocator
  {
    using value_type = T;
    using size_type = std::size_t;

    allocator() = default;

    template <class U> allocator(allocator<U> const&) {}

    template <class U> using rebind = allocator<U>;

    T* allocate(size_type n)
    {
      return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_type) { ::operator delete(p); }

    template <class U, class... Args> void construct(U* p, Args&&... args)
    {
      static_assert(std::is_same<U, int>::value, "");
      ++x;
      new (p) U(std::forward<Args>(args)...);
    }

    template <class U> void destroy(U* p)
    {
      static_assert(std::is_same<U, int>::value, "");
      ++x;
      p->~U();
    }

    bool operator==(allocator const&) const { return true; }
    bool operator!=(allocator const&) const { return false; }
  };
} // namespace set

UNORDERED_AUTO_TEST (allocator_construction_correctness) {
  {
    auto map = boost::unordered_map<int, int, std::hash<int>,
      std::equal_to<int>, map::allocator<std::pair<int const, int> > >();

    map.insert(std::make_pair(1337, 7331));
    map[1] = 2;
  }

  BOOST_TEST_EQ(x, 4);

  x = 0;

  {
    auto map = boost::unordered_multimap<int, int, std::hash<int>,
      std::equal_to<int>, map::allocator<std::pair<int const, int> > >();

    map.insert(std::make_pair(1337, 7331));
    map.insert(std::make_pair(1337, 7331));
  }

  BOOST_TEST_EQ(x, 4);

  x = 0;

  {
    auto set = boost::unordered_set<int, std::hash<int>, std::equal_to<int>,
      set::allocator<int> >();

    set.insert(1337);
    set.insert(7331);
  }

  BOOST_TEST_EQ(x, 4);

  x = 0;

  {
    auto set = boost::unordered_multiset<int, std::hash<int>,
      std::equal_to<int>, set::allocator<int> >();

    set.insert(1337);
    set.insert(1337);
  }

  BOOST_TEST_EQ(x, 4);
}

RUN_TESTS()
