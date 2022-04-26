// Copyright 2021 Christian Mazakas.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// clang-format off
#include "../helpers/prefix.hpp"
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include "../helpers/postfix.hpp"
// clang-format on

#include "../helpers/test.hpp"
#include "../objects/test.hpp"

#include <memory>

template <class C1, class C2> void scary_test()
{
  C1 x;
  C2 y;

  typename C2::iterator begin(x.begin());
  (void) (begin == x.end());
}

UNORDERED_AUTO_TEST (scary_tests) {
  typedef boost::unordered_map<int, int, boost::hash<int>, std::equal_to<int>,
    test::allocator1<std::pair<int const, int> > >
    unordered_map1;

  typedef boost::unordered_map<int, int, boost::hash<int>, std::equal_to<int>,
    std::allocator<std::pair<int const, int> > >
    unordered_map2;

  scary_test<unordered_map1, unordered_map2>();
}

RUN_TESTS()
