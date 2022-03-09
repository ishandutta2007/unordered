// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_UNORDERED_DETAIL_FCA_HPP
#define BOOST_UNORDERED_DETAIL_FCA_HPP

#include <boost/config.hpp>
#if defined(BOOST_HAS_PRAGMA_ONCE)
#pragma once
#endif

#include <boost/container/vector.hpp>
#include <boost/core/addressof.hpp>
#include <boost/core/alloc_construct.hpp>
#include <boost/core/allocator_access.hpp>
#include <boost/core/bit.hpp>
#include <boost/cstdint.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/move/core.hpp>

#include <algorithm>
#include <vector>

namespace boost {
  namespace unordered {
    namespace detail {
      namespace v2 {

        template <class = void> struct prime_fmod_size
        {
          static std::size_t sizes[];
          static std::size_t sizes_length;
          static boost::uint64_t inv_sizes[];

          static inline std::size_t size_index(std::size_t n)
          {
            // TODO: remove dependency on `lower_bound` here as it forces an
            // include of <algorithm> which is too large.
            //
            const std::size_t* bound =
              std::lower_bound(sizes, sizes + sizes_length, n);

            if (bound == (sizes + sizes_length))
              --bound;

            return static_cast<std::size_t>(bound - sizes);
          }

          static inline std::size_t size(std::size_t size_index)
          {
            return sizes[size_index];
          }

          // https://github.com/lemire/fastmod

#ifdef _MSC_VER
          static inline boost::uint64_t mul128_u32(
            boost::uint64_t lowbits, boost::uint32_t d)
          {
            return __umulh(lowbits, d);
          }
#else // _MSC_VER
          static inline boost::uint64_t mul128_u32(
            boost::uint64_t lowbits, boost::uint32_t d)
          {
            return ((__uint128_t)lowbits * d) >> 64;
          }
#endif

          static inline boost::uint32_t fastmod_u32(
            boost::uint32_t a, boost::uint64_t M, boost::uint32_t d)
          {
            boost::uint64_t lowbits = M * a;
            return (uint32_t)(mul128_u32(lowbits, d));
          }

          static inline std::size_t position(
            std::size_t hash, std::size_t size_index)
          {
            return fastmod_u32(uint32_t(hash) + uint32_t(hash >> 32),
              inv_sizes[size_index], uint32_t(sizes[size_index]));
          }
        };

        template <class T>
        std::size_t prime_fmod_size<T>::sizes[] = {53ul, 97ul, 193ul, 389ul,
          769ul, 1543ul, 3079ul, 6151ul, 12289ul, 24593ul, 49157ul, 98317ul,
          196613ul, 393241ul, 786433ul, 1572869ul, 3145739ul, 6291469ul,
          12582917ul, 25165843ul, 50331653ul, 100663319ul, 201326611ul,
          402653189ul, 805306457ul};

        template <class T>
        std::size_t
          prime_fmod_size<T>::sizes_length = sizeof(prime_fmod_size::sizes) /
                                             sizeof(prime_fmod_size::sizes[0]);

        template <class T>
        boost::uint64_t prime_fmod_size<T>::inv_sizes[] = {
          348051774975651918ull,
          190172619316593316ull,
          95578984837873325ull,
          47420935922132524ull,
          23987963684927896ull,
          11955116055547344ull,
          5991147799191151ull,
          2998982941588287ull,
          1501077717772769ull,
          750081082979285ull,
          375261795343686ull,
          187625172388393ull,
          93822606204624ull,
          46909513691883ull,
          23456218233098ull,
          11728086747027ull,
          5864041509391ull,
          2932024948977ull,
          1466014921160ull,
          733007198436ull,
          366503839517ull,
          183251896093ull,
          91625960335ull,
          45812983922ull,
          22906489714ull,
        };

        template <class Allocator> struct node
        {
          typedef
            typename boost::allocator_value_type<Allocator>::type value_type;
          typedef typename boost::allocator_pointer<Allocator>::type pointer;

          pointer next;
          value_type value;
        };

        template <class Allocator> struct bucket
        {
          typedef typename node<Allocator>::pointer node_pointer;

          node_pointer next;

          bucket() {}
          ~bucket() {}
        };

        template <class Bucket, class Allocator> struct bucket_group
        {
          typedef typename boost::allocator_rebind<Allocator, Bucket>::type
            bucket_allocator_type;
          typedef typename boost::allocator_pointer<bucket_allocator_type>::type
            bucket_pointer;

          typedef
            typename boost::allocator_rebind<Allocator, bucket_group>::type
              bucket_group_allocator_type;
          typedef
            typename boost::allocator_pointer<bucket_group_allocator_type>::type
              bucket_group_pointer;

          static const std::size_t N;

          bucket_pointer buckets;
          std::size_t bitmask;
          bucket_group_pointer next, prev;

          bucket_group() { bitmask = 0; }
          ~bucket_group() {}
        };

        template <class Bucket, class Allocator>
        const std::size_t
          bucket_group<Bucket, Allocator>::N = sizeof(std::size_t) * 8;

        template <class Bucket, class Allocator>
        struct grouped_bucket_iterator
            : public boost::iterator_facade<
                grouped_bucket_iterator<Bucket, Allocator>, Bucket,
                boost::forward_traversal_tag>
        {
          typedef typename boost::allocator_rebind<Allocator, Bucket>
            bucket_allocator_type;
          typedef typename boost::allocator_pointer<bucket_allocator_type>::type
            bucket_pointer;

          typedef typename boost::allocator_rebind<Allocator,
            bucket_group<Bucket, Allocator> >::type bucket_group_allocator_type;
          typedef
            typename boost::allocator_pointer<bucket_group_allocator_type>::type
              bucket_group_pointer;

        public:
          grouped_bucket_iterator() : p(), pbg() {}

        private:
          friend class boost::iterator_core_access;

          template <typename, typename, typename>
          friend class grouped_bucket_array;

          static const std::size_t N;

          grouped_bucket_iterator(
            Bucket* p_, bucket_group<Bucket, Allocator>* pbg_)
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
            std::size_t n = std::size_t(boost::core::countr_zero(
              pbg->bitmask & reset_first_bits((p - pbg->buckets) + 1)));

            if (n < N) {
              p = pbg->buckets + n;
            } else {
              pbg = pbg->next;
              p = pbg->buckets + boost::core::countr_zero(pbg->bitmask);
            }
          }

          bucket_pointer p;
          bucket_group_pointer pbg;
        };

        template <class Bucket, class Allocator>
        const std::size_t grouped_bucket_iterator<Bucket, Allocator>::N =
          bucket_group<Bucket, Allocator>::N;

        inline std::size_t set_bit(std::size_t n)
        {
          return std::size_t(1) << n;
        }

        inline std::size_t reset_bit(std::size_t n)
        {
          return ~(std::size_t(1) << n);
        }

        inline std::size_t reset_first_bits(std::size_t n) // n>0
        {
          return ~(~(std::size_t(0)) >> (sizeof(std::size_t) * 8 - n));
        }

        template <class T> struct span
        {
          T* begin() const BOOST_NOEXCEPT { return data; }
          T* end() const BOOST_NOEXCEPT { return data + size; }

          T* data;
          std::size_t size;
        };

        template <class Bucket, class Allocator, class SizePolicy>
        class grouped_bucket_array
        {
          BOOST_MOVABLE_BUT_NOT_COPYABLE(grouped_bucket_array)

          typedef SizePolicy size_policy;
          typedef node<Allocator> node_type;
          typedef typename boost::allocator_rebind<Allocator, node_type>::type
            node_allocator_type;
          typedef typename boost::allocator_pointer<node_allocator_type>::type
            node_pointer;

          typedef typename boost::allocator_rebind<Allocator,
            bucket<Allocator> >::type bucket_allocator_type;

          typedef typename boost::allocator_pointer<bucket_allocator_type>::type
            bucket_pointer;

          typedef bucket_group<bucket<Allocator>, Allocator> group;
          typedef typename boost::allocator_rebind<Allocator, group>::type
            group_allocator_type;

          typedef typename boost::allocator_pointer<group_allocator_type>::type
            group_pointer;

        public:
          typedef Bucket value_type;
          typedef std::size_t size_type;
          typedef Allocator allocator_type;
          typedef grouped_bucket_iterator<bucket<Allocator>, Allocator>
            iterator;

          grouped_bucket_array(size_type n, const Allocator& al)
              : size_index_(size_policy::size_index(n)),
                size_(size_policy::size(size_index_)), buckets(size_ + 1, al),
                groups(size_ / N + 1, al)
          {
            group& grp = groups.back();
            group_pointer pbg =
              boost::pointer_traits<group_pointer>::pointer_to(grp);

            pbg->buckets = boost::pointer_traits<bucket_pointer>::pointer_to(
              buckets[N * (size_ / N)]);
            pbg->bitmask = set_bit(size_ % N);
            pbg->next = pbg->prev = pbg;
          }

          ~grouped_bucket_array() {}

          grouped_bucket_array(BOOST_RV_REF(grouped_bucket_array)
              other) BOOST_NOEXCEPT : size_index_(other.size_index_),
                                      size_(other.size_),
                                      buckets(boost::move(other.buckets)),
                                      groups(boost::move(other.groups))
          {
            other.size_ = 0;
            other.size_index_ = 0;
          }

          grouped_bucket_array& operator=(
            BOOST_RV_REF(grouped_bucket_array) other) BOOST_NOEXCEPT
          {
            size_index_ = other.size_index_;
            size_ = other.size_;
            buckets = boost::move(other.buckets);
            groups = boost::move(other.groups);
            return *this;
          };

          iterator begin() const { return ++at(size_); }

          iterator end() const
          {
            // micro optimization: no need to return the bucket group
            // as end() is not incrementable
            iterator pbg;
            pbg.p = const_cast<bucket<Allocator>*>(&buckets.back());

            return pbg;
          }

          size_type capacity() const BOOST_NOEXCEPT { return size_; }

          iterator at(size_type n) const
          {
            iterator pbg(const_cast<bucket<Allocator>*>(&buckets[n]),
              const_cast<group*>(&groups[n / N]));

            return pbg;
          }

          span<bucket<Allocator> > raw()
          {
            return span<bucket<Allocator> >(buckets.data(), size_);
          }

          size_type position(std::size_t hash) const
          {
            return size_policy::position(hash, size_index_);
          }

          void insert_node(iterator itb, node_pointer p) BOOST_NOEXCEPT
          {
            if (!itb->next) { // empty bucket
              typename iterator::bucket_pointer pb = itb.p;
              typename iterator::bucket_group_pointer pbg = itb.pbg;

              std::size_t n = pb - &buckets[0];
              if (!pbg->bitmask) { // empty group
                pbg->buckets = &buckets[N * (n / N)];
                pbg->next = groups.back().next;
                pbg->next->prev = pbg;
                pbg->prev = &groups.back();
                pbg->prev->next = pbg;
              }
              pbg->bitmask |= set_bit(n % N);
            }
            p->next = itb->next;
            itb->next = p;
          }

          void extract_node(iterator itb, node_pointer p) BOOST_NOEXCEPT
          {
            node_pointer* pp = &itb->next;
            while ((*pp) != p)
              pp = &(*pp)->next;
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
            bucket_group<bucket<Allocator>, Allocator>*pbg = &groups.front(),
                                            last = &groups.back();
            for (; pbg != last; ++pbg) {
              for (std::size_t n = 0; n < N; ++n) {
                if (!pbg->buckets[n].next)
                  pbg->bitmask &= reset_bit(n);
              }
              if (!pbg->bitmask && pbg->next)
                unlink_group(pbg);
            }
            for (std::size_t n = 0; n < size_ % N;
                 ++n) { // do not check end bucket
              if (!pbg->buckets[n].next)
                pbg->bitmask &= reset_bit(n);
            }
          }

        private:
          static const std::size_t N = group::N;

          void unlink_bucket(iterator itb)
          {
            typename iterator::bucket_pointer p = itb.p;
            typename iterator::bucket_group_pointer pbg = itb.pbg;
            if (!(pbg->bitmask &= reset_bit(p - pbg->buckets)))
              unlink_group(pbg);
          }

          void unlink_group(group_pointer pbg)
          {
            pbg->next->prev = pbg->prev;
            pbg->prev->next = pbg->next;
            pbg->prev = pbg->next = group_pointer();
          }

          std::size_t size_index_, size_;
          boost::container::vector<bucket<Allocator>, bucket_allocator_type>
            buckets;
          boost::container::vector<group, group_allocator_type> groups;
        };

        // struct grouped_buckets
        // {
        //   static constexpr bool has_constant_iterator_increment = true;

        //   template <typename Allocator, typename SizePolicy, typename
        //   Payload> using array_type =
        //     grouped_bucket_array<Allocator, SizePolicy, Payload>;
        // };
      } // namespace v2
    }   // namespace detail
  }     // namespace unordered
} // namespace boost

#endif // BOOST_UNORDERED_DETAIL_FCA_HPP
