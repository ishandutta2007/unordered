// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_UNORDERED_DETAIL_FCA_HPP
#define BOOST_UNORDERED_DETAIL_FCA_HPP

#include <boost/config.hpp>
#if defined(BOOST_HAS_PRAGMA_ONCE)
#pragma once
#endif

#include <boost/align/alignment_of.hpp>
#include <boost/core/addressof.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/bit.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/cstdint.hpp>

// `iterator_facade` has transitive dependencies on Boost.MPL; one of the
// headers is generating a `-Wsign-conversion` warning which has an open PR to
// address the issue but merging does not seem likely so for now create a rote
// workaround.
//
// TODO: eventually remove this once MPL is fixed or we decide to migrate off of
// the Boost.Iterator dependency.
//
#if defined(BOOST_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#include <boost/iterator/iterator_facade.hpp>
#pragma GCC diagnostic pop
#else
#include <boost/iterator/iterator_facade.hpp>
#endif

#include <boost/move/core.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/swap.hpp>
#include <boost/type_traits/aligned_storage.hpp>

#include <climits>

namespace boost {
  namespace unordered {
    namespace detail {

#if defined(SIZE_MAX)
#if ((((SIZE_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define BOOST_UNORDERED_FCA_HAS_64B_SIZE_T
#endif
#elif defined(UINTPTR_MAX) /* used as proxy for std::size_t */
#if ((((UINTPTR_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define BOOST_UNORDERED_FCA_HAS_64B_SIZE_T
#endif
#endif

#if !defined(BOOST_NO_INT64_T) &&                                              \
  (defined(BOOST_HAS_INT128) || (defined(BOOST_MSVC) && defined(_WIN64)))
#define BOOST_UNORDERED_FCA_FASTMOD_SUPPORT
#endif

      template <class = void> struct prime_fmod_size
      {
        // Because we compile for C++03, we don't have access to any inline
        // initialization for array data members so the definitions must exist
        // out-of-line. To keep the library header-only, we introduce a dummy
        // template parameter which permits the definition to be included in
        // multiple TUs without conflict.
        //
        static std::size_t sizes[];
        static std::size_t const sizes_len;
        static std::size_t (*positions[])(std::size_t);

#if defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT)
        static uint64_t inv_sizes32[];
        static std::size_t const inv_sizes32_len;
#endif /* defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT) */

        static inline std::size_t size_index(std::size_t n)
        {
          std::size_t i = 0;
          for (; i < (sizes_len - 1); ++i) {
            if (sizes[i] >= n) {
              break;
            }
          }
          return i;
        }

        static inline std::size_t size(std::size_t size_index)
        {
          return sizes[size_index];
        }

        template <std::size_t Size> static std::size_t modulo(std::size_t hash)
        {
          return hash % Size;
        }

#if defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT)
        // https://github.com/lemire/fastmod

#if defined(_MSC_VER)
        static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
        {
          return __umulh(lowbits, d);
        }
#else
        static inline uint64_t mul128_u32(uint64_t lowbits, uint32_t d)
        {
          __extension__ typedef unsigned __int128 uint128;
          return static_cast<uint64_t>(((uint128)lowbits * d) >> 64);
        }
#endif /* defined(_MSC_VER) */

        static inline uint32_t fastmod_u32(uint32_t a, uint64_t M, uint32_t d)
        {
          uint64_t lowbits = M * a;
          return (uint32_t)(mul128_u32(lowbits, d));
        }
#endif /* defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT) */

        static inline std::size_t position(
          std::size_t hash, std::size_t size_index)
        {
#if defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT)
#if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
          std::size_t sizes_under_32bit = inv_sizes32_len;
          if (BOOST_LIKELY(size_index < sizes_under_32bit)) {
            return fastmod_u32(uint32_t(hash) + uint32_t(hash >> 32),
              inv_sizes32[size_index], uint32_t(sizes[size_index]));
          } else {
            return positions[size_index - sizes_under_32bit](hash);
          }
#else
          return fastmod_u32(
            hash, inv_sizes32[size_index], uint32_t(sizes[size_index]));
#endif /* defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T) */
#else
          return positions[size_index](hash);
#endif /* defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT) */
        }
      }; // prime_fmod_size

#define BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT_INCOMPLETE                      \
  (13ul)(29ul)(53ul)(97ul)(193ul)(389ul)(769ul)(1543ul)(3079ul)(6151ul)(       \
    12289ul)(24593ul)(49157ul)(98317ul)(196613ul)(393241ul)(786433ul)(         \
    1572869ul)(3145739ul)(6291469ul)(12582917ul)(25165843ul)(50331653ul)(      \
    100663319ul)(201326611ul)(402653189ul)(805306457ul)(1610612741ul)(         \
    3221225473ul)

#if !defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)

#define BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT                                 \
  BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT_INCOMPLETE(4294967291ul)

#define BOOST_UNORDERED_PRIME_FMOD_SIZES_64BIT

#else

#define BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT                                 \
  BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT_INCOMPLETE

// The original sequence here is this:
// (6442450939ul)
// (12884901893ul)
// (25769803751ul)
// (51539607551ul)
// (103079215111ul)
// (206158430209ul)
// (412316860441ul)
// (824633720831ul)
// (1649267441651ul)
//
// but this causes problems on versions of mingw where the `long` type is 32
// bits, even for 64-bit targets. We work around this by replacing the literals
// with compile-time arithmetic, using bitshifts to reconstruct the number.
//

// clang-format off
#define BOOST_UNORDERED_PRIME_FMOD_SIZES_64BIT                                   \
  ((boost::ulong_long_type(1ul) << 32)   + boost::ulong_long_type(2147483643ul)) \
  ((boost::ulong_long_type(3ul) << 32)   + boost::ulong_long_type(5ul))          \
  ((boost::ulong_long_type(5ul) << 32)   + boost::ulong_long_type(4294967271ul)) \
  ((boost::ulong_long_type(11ul) << 32)  + boost::ulong_long_type(4294967295ul)) \
  ((boost::ulong_long_type(24ul) << 32)  + boost::ulong_long_type(7ul))          \
  ((boost::ulong_long_type(48ul) << 32)  + boost::ulong_long_type(1ul))          \
  ((boost::ulong_long_type(96ul) << 32)  + boost::ulong_long_type(25ul))         \
  ((boost::ulong_long_type(191ul) << 32) + boost::ulong_long_type(4294967295ul)) \
  ((boost::ulong_long_type(383ul) << 32) + boost::ulong_long_type(4294967283ul))
      // clang-format on

#endif /* BOOST_UNORDERED_FCA_HAS_64B_SIZE_T */

#define BOOST_UNORDERED_PRIME_FMOD_SIZES                                       \
  BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT BOOST_UNORDERED_PRIME_FMOD_SIZES_64BIT

      template <class T>
      std::size_t prime_fmod_size<T>::sizes[] = {
        BOOST_PP_SEQ_ENUM(BOOST_UNORDERED_PRIME_FMOD_SIZES)};

      template <class T>
      std::size_t const prime_fmod_size<T>::sizes_len = BOOST_PP_SEQ_SIZE(
        BOOST_UNORDERED_PRIME_FMOD_SIZES);

// Similarly here, we have to re-express the integer initialization using
// arithmetic such that each literal can fit in a 32-bit value.
//
#if defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT)
      // clang-format off
        template <class T>
        uint64_t prime_fmod_size<T>::inv_sizes32[] = {
          (boost::ulong_long_type(330382099ul) << 32) + boost::ulong_long_type(2973438898ul) /* = 1418980313362273202 */,
          (boost::ulong_long_type(148102320ul) << 32) + boost::ulong_long_type(2369637129ul) /* = 636094623231363849 */,
          (boost::ulong_long_type(81037118ul) << 32)  + boost::ulong_long_type(3403558990ul) /* = 348051774975651918 */,
          (boost::ulong_long_type(44278013ul) << 32)  + boost::ulong_long_type(1549730468ul) /* = 190172619316593316 */,
          (boost::ulong_long_type(22253716ul) << 32)  + boost::ulong_long_type(2403401389ul) /* = 95578984837873325 */,
          (boost::ulong_long_type(11041047ul) << 32)  + boost::ulong_long_type(143533612ul)  /* = 47420935922132524 */,
          (boost::ulong_long_type(5585133ul) << 32)   + boost::ulong_long_type(106117528ul)  /* = 23987963684927896 */,
          (boost::ulong_long_type(2783517ul) << 32)   + boost::ulong_long_type(1572687312ul) /* = 11955116055547344 */,
          (boost::ulong_long_type(1394922ul) << 32)   + boost::ulong_long_type(3428720239ul) /* = 5991147799191151 */,
          (boost::ulong_long_type(698255ul) << 32)    + boost::ulong_long_type(552319807ul)  /* = 2998982941588287 */,
          (boost::ulong_long_type(349496ul) << 32)    + boost::ulong_long_type(3827689953ul) /* = 1501077717772769 */,
          (boost::ulong_long_type(174641ul) << 32)    + boost::ulong_long_type(3699438549ul) /* = 750081082979285 */,
          (boost::ulong_long_type(87372ul) << 32)     + boost::ulong_long_type(1912757574ul) /* = 375261795343686 */,
          (boost::ulong_long_type(43684ul) << 32)     + boost::ulong_long_type(3821029929ul) /* = 187625172388393 */,
          (boost::ulong_long_type(21844ul) << 32)     + boost::ulong_long_type(3340590800ul) /* = 93822606204624 */,
          (boost::ulong_long_type(10921ul) << 32)     + boost::ulong_long_type(4175852267ul) /* = 46909513691883 */,
          (boost::ulong_long_type(5461ul) << 32)      + boost::ulong_long_type(1401829642ul) /* = 23456218233098 */,
          (boost::ulong_long_type(2730ul) << 32)      + boost::ulong_long_type(2826028947ul) /* = 11728086747027 */,
          (boost::ulong_long_type(1365ul) << 32)      + boost::ulong_long_type(1411150351ul) /* = 5864041509391 */,
          (boost::ulong_long_type(682ul) << 32)       + boost::ulong_long_type(2857253105ul) /* = 2932024948977 */,
          (boost::ulong_long_type(341ul) << 32)       + boost::ulong_long_type(1431073224ul) /* = 1466014921160 */,
          (boost::ulong_long_type(170ul) << 32)       + boost::ulong_long_type(2862758116ul) /* = 733007198436 */,
          (boost::ulong_long_type(85ul) << 32)        + boost::ulong_long_type(1431619357ul) /* = 366503839517 */,
          (boost::ulong_long_type(42ul) << 32)        + boost::ulong_long_type(2863269661ul) /* = 183251896093 */,
          (boost::ulong_long_type(21ul) << 32)        + boost::ulong_long_type(1431647119ul) /* = 91625960335 */,
          (boost::ulong_long_type(10ul) << 32)        + boost::ulong_long_type(2863310962ul) /* = 45812983922 */,
          (boost::ulong_long_type(5ul) << 32)         + boost::ulong_long_type(1431653234ul) /* = 22906489714 */,
          (boost::ulong_long_type(2ul) << 32)         + boost::ulong_long_type(2863311496ul) /* = 11453246088 */,
          (boost::ulong_long_type(1ul) << 32)         + boost::ulong_long_type(1431655764ul) /* = 5726623060 */,
#if !defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
        };
#else
          (boost::ulong_long_type(1ul) << 32)         + boost::ulong_long_type(6ul)          /* 4294967302 */
        };
      // clang-format on
#endif /* !defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T) */

      template <class T>
      std::size_t const
        prime_fmod_size<T>::inv_sizes32_len = sizeof(inv_sizes32) /
                                              sizeof(inv_sizes32[0]);

#endif /* defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT) */

#define BOOST_UNORDERED_PRIME_FMOD_POSITIONS_ELEMENT(z, _, n)                  \
  prime_fmod_size<T>::template modulo<n>,

      template <class T>
      std::size_t (*prime_fmod_size<T>::positions[])(std::size_t) = {
#if !defined(BOOST_UNORDERED_FCA_FASTMOD_SUPPORT)
        BOOST_PP_SEQ_FOR_EACH(BOOST_UNORDERED_PRIME_FMOD_POSITIONS_ELEMENT, ~,
          BOOST_UNORDERED_PRIME_FMOD_SIZES_32BIT)
#endif

#if defined(BOOST_UNORDERED_FCA_HAS_64B_SIZE_T)
          BOOST_PP_SEQ_FOR_EACH(BOOST_UNORDERED_PRIME_FMOD_POSITIONS_ELEMENT, ~,
            BOOST_UNORDERED_PRIME_FMOD_SIZES_64BIT)
#endif
      };

#undef BOOST_UNORDERED_PRIME_FMOD_POSITIONS_ELEMENT
#undef BOOST_UNORDERED_PRIME_FMOD_SIZES
#undef BOOST_UNORDERED_PRIME_FMOD_SIZES_64BIT
#undef BOOST_UNORDERED_PRIME_FMOD_SIZES_34BIT
#undef BOOST_UNORDERED_PRIME_FMOD_SIZES_34BIT_INCOMPLETE

#ifdef BOOST_UNORDERED_FCA_FASTMOD_SUPPORT
#undef BOOST_UNORDERED_FCA_FASTMOD_SUPPORT
#endif

#ifdef BOOST_UNORDERED_FCA_HAS_64B_SIZE_T
#undef BOOST_UNORDERED_FCA_HAS_64B_SIZE_T
#endif

      template <class ValueType, class VoidPtr> struct node
      {
        typedef ValueType value_type;
        typedef typename boost::pointer_traits<VoidPtr>::template rebind_to<
          node>::type node_pointer;

        node_pointer next;
        typename boost::aligned_storage<sizeof(value_type),
          boost::alignment_of<value_type>::value>::type buf;

        node() BOOST_NOEXCEPT : next(), buf() {}

        value_type* value_ptr() BOOST_NOEXCEPT
        {
          return reinterpret_cast<value_type*>(buf.address());
        }

        value_type& value() BOOST_NOEXCEPT
        {
          return *reinterpret_cast<value_type*>(buf.address());
        }
      };

      template <class Node> struct bucket
      {
        typedef typename Node::node_pointer node_pointer;
        typedef typename boost::pointer_traits<
          node_pointer>::template rebind_to<bucket>::type bucket_pointer;

        node_pointer next;

        bucket() BOOST_NOEXCEPT : next() {}
      };

      template <class Bucket> struct bucket_group
      {
        typedef typename Bucket::bucket_pointer bucket_pointer;
        typedef
          typename boost::pointer_traits<bucket_pointer>::template rebind_to<
            bucket_group>::type bucket_group_pointer;

        BOOST_STATIC_CONSTANT(std::size_t, N = sizeof(std::size_t) * CHAR_BIT);

        bucket_pointer buckets;
        std::size_t bitmask;
        bucket_group_pointer next, prev;

        bucket_group() BOOST_NOEXCEPT : buckets(), bitmask(0), next(), prev() {}
        ~bucket_group() {}
      };

      inline std::size_t set_bit(std::size_t n) { return std::size_t(1) << n; }

      inline std::size_t reset_bit(std::size_t n)
      {
        return ~(std::size_t(1) << n);
      }

      inline std::size_t reset_first_bits(std::size_t n) // n>0
      {
        return ~(~(std::size_t(0)) >> (sizeof(std::size_t) * 8 - n));
      }

      template <class Bucket>
      struct grouped_bucket_iterator
          : public boost::iterator_facade<grouped_bucket_iterator<Bucket>,
              Bucket, boost::forward_traversal_tag>
      {
      public:
        typedef typename Bucket::bucket_pointer bucket_pointer;
        typedef
          typename boost::pointer_traits<bucket_pointer>::template rebind_to<
            bucket_group<Bucket> >::type bucket_group_pointer;

      private:
        bucket_pointer p;
        bucket_group_pointer pbg;

      public:
        grouped_bucket_iterator() : p(), pbg() {}

      private:
        friend class boost::iterator_core_access;

        template <typename, typename, typename>
        friend class grouped_bucket_array;

        BOOST_STATIC_CONSTANT(std::size_t, N = bucket_group<Bucket>::N);

        grouped_bucket_iterator(bucket_pointer p_, bucket_group_pointer pbg_)
            : p(p_), pbg(pbg_)
        {
        }

        Bucket& dereference() const BOOST_NOEXCEPT { return *p; }

        bool equal(const grouped_bucket_iterator& x) const BOOST_NOEXCEPT
        {
          return p == x.p;
        }

        void increment() BOOST_NOEXCEPT
        {
          std::size_t const offset = static_cast<std::size_t>(p - pbg->buckets);

          std::size_t n = std::size_t(boost::core::countr_zero(
            pbg->bitmask & reset_first_bits(offset + 1)));

          if (n < N) {
            p = pbg->buckets + n;
          } else {
            pbg = pbg->next;
            p = pbg->buckets + static_cast<std::ptrdiff_t>(
                                 boost::core::countr_zero(pbg->bitmask));
          }
        }
      };

      template <class Node> struct const_grouped_local_bucket_iterator;

      template <class Node>
      struct grouped_local_bucket_iterator
          : public boost::iterator_facade<grouped_local_bucket_iterator<Node>,
              typename Node::value_type, boost::forward_traversal_tag>
      {

        typedef typename Node::node_pointer node_pointer;

      public:
        typedef typename Node::value_type value_type;
        typedef value_type element_type;
        typedef value_type* pointer;
        typedef value_type& reference;
        typedef std::ptrdiff_t difference_type;
        typedef std::forward_iterator_tag iterator_category;

        grouped_local_bucket_iterator() : p() {}

      private:
        friend class boost::iterator_core_access;

        template <typename, typename, typename>
        friend class grouped_bucket_array;

        template <class> friend struct const_grouped_local_bucket_iterator;

        grouped_local_bucket_iterator(node_pointer p_) : p(p_) {}

        value_type& dereference() const BOOST_NOEXCEPT { return p->value(); }

        bool equal(const grouped_local_bucket_iterator& x) const BOOST_NOEXCEPT
        {
          return p == x.p;
        }

        void increment() BOOST_NOEXCEPT { p = p->next; }

        node_pointer p;
      };

      template <class Node>
      struct const_grouped_local_bucket_iterator
          : public boost::iterator_facade<
              const_grouped_local_bucket_iterator<Node>,
              typename Node::value_type const, boost::forward_traversal_tag>
      {
        typedef typename Node::node_pointer node_pointer;

      public:
        typedef typename Node::value_type const value_type;
        typedef value_type const element_type;
        typedef value_type const* pointer;
        typedef value_type const& reference;
        typedef std::ptrdiff_t difference_type;
        typedef std::forward_iterator_tag iterator_category;

        const_grouped_local_bucket_iterator() : p() {}
        const_grouped_local_bucket_iterator(
          grouped_local_bucket_iterator<Node> it)
            : p(it.p)
        {
        }

      private:
        friend class boost::iterator_core_access;

        template <typename, typename, typename>
        friend class grouped_bucket_array;

        const_grouped_local_bucket_iterator(node_pointer p_) : p(p_) {}

        value_type& dereference() const BOOST_NOEXCEPT { return p->value(); }

        bool equal(
          const const_grouped_local_bucket_iterator& x) const BOOST_NOEXCEPT
        {
          return p == x.p;
        }

        void increment() BOOST_NOEXCEPT { p = p->next; }

        node_pointer p;
      };

      template <class T> struct span
      {
        T* begin() const BOOST_NOEXCEPT { return data; }
        T* end() const BOOST_NOEXCEPT { return data + size; }

        T* data;
        std::size_t size;

        span(T* data_, std::size_t size_) : data(data_), size(size_) {}
      };

      template <class Bucket, class Allocator, class SizePolicy>
      class grouped_bucket_array
      {
        BOOST_MOVABLE_BUT_NOT_COPYABLE(grouped_bucket_array)

        typedef typename boost::allocator_value_type<Allocator>::type
          allocator_value_type;
        typedef
          typename boost::allocator_void_pointer<Allocator>::type void_pointer;

      public:
        typedef typename boost::allocator_rebind<Allocator,
          node<allocator_value_type, void_pointer> >::type node_allocator_type;

        typedef node<allocator_value_type, void_pointer> node_type;
        typedef typename boost::allocator_pointer<node_allocator_type>::type
          node_pointer;
        typedef SizePolicy size_policy;

      private:
        typedef typename boost::allocator_rebind<Allocator, Bucket>::type
          bucket_allocator_type;
        typedef typename boost::allocator_pointer<bucket_allocator_type>::type
          bucket_pointer;
        typedef boost::pointer_traits<bucket_pointer> bucket_pointer_traits;

        typedef bucket_group<Bucket> group;
        typedef typename boost::allocator_rebind<Allocator, group>::type
          group_allocator_type;
        typedef typename boost::allocator_pointer<group_allocator_type>::type
          group_pointer;
        typedef typename boost::pointer_traits<group_pointer>
          group_pointer_traits;

      public:
        typedef Bucket value_type;
        typedef Bucket bucket_type;
        typedef std::size_t size_type;
        typedef Allocator allocator_type;
        typedef grouped_bucket_iterator<Bucket> iterator;
        typedef grouped_local_bucket_iterator<node_type> local_iterator;
        typedef const_grouped_local_bucket_iterator<node_type>
          const_local_iterator;

      private:
        Allocator allocator;
        node_allocator_type node_allocator;

        std::size_t size_index_, size_;
        bucket_pointer buckets;
        group_pointer groups;

      public:
        grouped_bucket_array(size_type n, const Allocator& al)
            : allocator(al), node_allocator(al),
              size_index_(size_policy::size_index(n)),
              size_(size_policy::size(size_index_)), buckets(), groups()
        {
          bucket_allocator_type bucket_alloc = this->get_bucket_allocator();
          group_allocator_type group_alloc = this->get_group_allocator();

          size_type const num_buckets = buckets_len();
          size_type const num_groups = groups_len();

          buckets = boost::allocator_allocate(bucket_alloc, num_buckets);
          BOOST_TRY
          {
            groups = boost::allocator_allocate(group_alloc, num_groups);

            bucket_type* pb = boost::to_address(buckets);
            for (size_type i = 0; i < num_buckets; ++i) {
              new (pb + i) bucket_type();
            }

            group* pg = boost::to_address(groups);
            for (size_type i = 0; i < num_groups; ++i) {
              new (pg + i) group();
            }
          }
          BOOST_CATCH(...)
          {
            boost::allocator_deallocate(bucket_alloc, buckets, num_buckets);
            BOOST_RETHROW
          }
          BOOST_CATCH_END

          size_type const N = group::N;
          group_pointer pbg = groups + (num_groups - 1);

          pbg->buckets = buckets + N * (size_ / N);
          pbg->bitmask = set_bit(size_ % N);
          pbg->next = pbg->prev = pbg;
        }

        ~grouped_bucket_array() { this->deallocate(); }

        grouped_bucket_array(
          BOOST_RV_REF(grouped_bucket_array) other) BOOST_NOEXCEPT
            : allocator(boost::move(other.allocator)),
              node_allocator(boost::move(other.node_allocator)),
              size_index_(other.size_index_),
              size_(other.size_),
              buckets(other.buckets),
              groups(other.groups)
        {
          other.size_ = 0;
          other.size_index_ = 0;
          other.buckets = bucket_pointer();
          other.groups = group_pointer();
        }

        grouped_bucket_array& operator=(
          BOOST_RV_REF(grouped_bucket_array) other) BOOST_NOEXCEPT
        {
          BOOST_ASSERT(this->get_allocator() == other.get_allocator());

          if (this == &other) {
            return *this;
          }

          this->deallocate();
          size_index_ = other.size_index_;
          size_ = other.size_;

          buckets = other.buckets;
          groups = other.groups;

          other.size_index_ = 0;
          other.size_ = 0;
          other.buckets = bucket_pointer();
          other.groups = group_pointer();

          return *this;
        }

        void deallocate() BOOST_NOEXCEPT
        {
          if (buckets) {
            bucket_allocator_type bucket_alloc = this->get_bucket_allocator();
            boost::allocator_deallocate(
              bucket_alloc, buckets, this->buckets_len());

            buckets = bucket_pointer();
          }

          if (groups) {
            group_allocator_type group_alloc = this->get_group_allocator();
            boost::allocator_deallocate(
              group_alloc, groups, this->groups_len());

            groups = group_pointer();
          }
        }

        void swap(grouped_bucket_array& other)
        {
          std::swap(size_index_, other.size_index_);
          std::swap(size_, other.size_);
          std::swap(buckets, other.buckets);
          std::swap(groups, other.groups);

          bool b = boost::allocator_propagate_on_container_swap<
            allocator_type>::type::value;
          if (b) {
            boost::swap(allocator, other.allocator);
            boost::swap(node_allocator, other.node_allocator);
          }
        }

        Allocator& get_allocator() BOOST_NOEXCEPT { return allocator; }
        Allocator const& get_const_allocator() const BOOST_NOEXCEPT
        {
          return allocator;
        }

        node_allocator_type const& get_node_allocator() const
        {
          return node_allocator;
        }

        node_allocator_type& get_node_allocator() { return node_allocator; }

        bucket_allocator_type get_bucket_allocator() const
        {
          return this->get_const_allocator();
        }

        group_allocator_type get_group_allocator() const
        {
          return this->get_const_allocator();
        }

        size_type buckets_len() const BOOST_NOEXCEPT { return size_ + 1; }

        size_type groups_len() const BOOST_NOEXCEPT
        {
          return size_ / group::N + 1;
        }

        void reset_allocator(Allocator const& allocator_)
        {
          allocator = allocator_;
          node_allocator = node_allocator_type(allocator);
        }

        size_type bucket_count() const { return size_; }

        iterator begin() const
        {
          if (size_ == 0) {
            return end();
          }
          return ++at(size_);
        }

        iterator end() const
        {
          // micro optimization: no need to return the bucket group
          // as end() is not incrementable
          iterator pbg;
          if (size_) {
            pbg.p = buckets + (this->buckets_len() - 1);
          }

          return pbg;
        }

        local_iterator begin(size_type n) const
        {
          return local_iterator((buckets + n)->next);
        }

        local_iterator end(size_type) const { return local_iterator(); }

        size_type capacity() const BOOST_NOEXCEPT { return size_; }

        iterator at(size_type n) const
        {
          std::size_t const N = group::N;

          iterator pbg(buckets + n, groups + (n / N));

          return pbg;
        }

        span<Bucket> raw()
        {
          BOOST_ASSERT(size_ == 0 || size_ < this->buckets_len());
          return span<Bucket>(boost::to_address(buckets), size_);
        }

        size_type position(std::size_t hash) const
        {
          return size_policy::position(hash, size_index_);
        }

        void clear()
        {
          this->deallocate();
          size_index_ = 0;
          size_ = 0;
        }

        void insert_node(iterator itb, node_pointer p) BOOST_NOEXCEPT
        {
          std::size_t const N = group::N;

          if (!itb->next) { // empty bucket
            typename iterator::bucket_pointer pb = itb.p;
            typename iterator::bucket_group_pointer pbg = itb.pbg;

            std::size_t n =
              static_cast<std::size_t>(boost::to_address(pb) - &buckets[0]);

            if (!pbg->bitmask) { // empty group
              size_type const num_groups = this->groups_len();

              pbg->buckets = buckets + (N * (n / N));
              pbg->next = (groups + (num_groups - 1))->next;
              pbg->next->prev = pbg;
              pbg->prev = groups + (num_groups - 1);
              pbg->prev->next = pbg;
            }
            pbg->bitmask |= set_bit(n % N);
          }
          p->next = itb->next;
          itb->next = p;
        }

        void insert_node_hint(
          iterator itb, node_pointer p, node_pointer hint) BOOST_NOEXCEPT
        {
          std::size_t const N = group::N;

          if (!itb->next) { // empty bucket
            typename iterator::bucket_pointer pb = itb.p;
            typename iterator::bucket_group_pointer pbg = itb.pbg;

            std::size_t n =
              static_cast<std::size_t>(boost::to_address(pb) - &buckets[0]);
            if (!pbg->bitmask) { // empty group
              size_type const num_groups = this->groups_len();

              pbg->buckets = buckets + (N * (n / N));
              pbg->next = (groups + (num_groups - 1))->next;
              pbg->next->prev = pbg;
              pbg->prev = groups + (num_groups - 1);
              pbg->prev->next = pbg;
            }
            pbg->bitmask |= set_bit(n % N);
          }

          if (hint) {
            p->next = hint->next;
            hint->next = p;
          } else {
            p->next = itb->next;
            itb->next = p;
          }
        }

        void extract_node(iterator itb, node_pointer p) BOOST_NOEXCEPT
        {
          node_pointer* pp = boost::addressof(itb->next);
          while ((*pp) != p)
            pp = boost::addressof((*pp)->next);
          *pp = p->next;
          if (!itb->next)
            unlink_bucket(itb);
        }

        void extract_node_after(iterator itb, node_pointer* pp) BOOST_NOEXCEPT
        {
          *pp = (*pp)->next;
          if (!itb->next)
            unlink_bucket(itb);
        }

        void unlink_empty_buckets() BOOST_NOEXCEPT
        {
          std::size_t const N = group::N;

          group_pointer pbg = groups, last = groups + (this->groups_len() - 1);

          for (; pbg != last; ++pbg) {
            if (!pbg->buckets) {
              continue;
            }

            for (std::size_t n = 0; n < N; ++n) {
              bucket_pointer bs = pbg->buckets;
              bucket_type& b = bs[static_cast<std::ptrdiff_t>(n)];
              if (!b.next)
                pbg->bitmask &= reset_bit(n);
            }
            if (!pbg->bitmask && pbg->next)
              unlink_group(pbg);
          }

          // do not check end bucket
          for (std::size_t n = 0; n < size_ % N; ++n) {
            if (!pbg->buckets[static_cast<std::ptrdiff_t>(n)].next)
              pbg->bitmask &= reset_bit(n);
          }
        }

      private:
        void unlink_bucket(iterator itb)
        {
          typename iterator::bucket_pointer p = itb.p;
          typename iterator::bucket_group_pointer pbg = itb.pbg;
          if (!(pbg->bitmask &=
                reset_bit(static_cast<std::size_t>(p - pbg->buckets))))
            unlink_group(pbg);
        }

        void unlink_group(group_pointer pbg)
        {
          pbg->next->prev = pbg->prev;
          pbg->prev->next = pbg->next;
          pbg->prev = pbg->next = group_pointer();
        }
      };
    } // namespace detail
  }   // namespace unordered
} // namespace boost

#endif // BOOST_UNORDERED_DETAIL_FCA_HPP
