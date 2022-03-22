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
#include <boost/core/default_allocator.hpp>
#include <boost/core/empty_value.hpp>
#include <boost/cstdint.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/move/core.hpp>

#include <algorithm>
#include <vector>

namespace boost {
  namespace unordered {
    namespace detail {
      namespace v2 {

        template <class T, class Allocator = boost::default_allocator<T> >
        struct dynamic_array
            : boost::empty_value<
                typename boost::allocator_rebind<Allocator, T>::type>
        {
        private:
          BOOST_COPYABLE_AND_MOVABLE(dynamic_array)

        public:
          typedef
            typename boost::allocator_rebind<Allocator, T>::type allocator_type;
          typedef
            typename boost::allocator_size_type<allocator_type>::type size_type;
          typedef T value_type;

        private:
          typedef
            typename boost::allocator_pointer<allocator_type>::type pointer;

          pointer p_;
          size_type len_;

        public:
          dynamic_array(size_type n, allocator_type a = allocator_type())
              : empty_value<allocator_type>(boost::empty_init_t(), a), p_(),
                len_(n)
          {
            p_ = boost::allocator_allocate(a, len_);
            boost::allocator_construct_n(a, boost::to_address(p_), len_);
          }

          dynamic_array(dynamic_array const& other)
              : empty_value<allocator_type>(
                  boost::empty_init_t(), other.allocator()),
                p_(), len_(0)
          {
            size_type len = other.size();
            allocator_type a = allocator();
            p_ = boost::allocator_allocate(a, len);
            boost::allocator_construct_n(a, data(), len, other.data());
            len_ = len;
          }

          dynamic_array(BOOST_RV_REF(dynamic_array) other) BOOST_NOEXCEPT
              : boost::empty_value<allocator_type>(boost::empty_init_t(),
                  boost::move(
                    static_cast<boost::empty_value<allocator_type> >(other)
                      .get()))
          {
            p_ = other.p_;
            len_ = other.len_;

            other.p_ = pointer();
            other.len_ = 0;
          }

          ~dynamic_array() { deallocate(); }

          dynamic_array& operator=(BOOST_RV_REF(dynamic_array) other)
          {
            typedef
              typename boost::allocator_propagate_on_container_move_assignment<
                allocator_type>::type propagate;

            if ((allocator() == other.allocator()) || propagate::value) {

              deallocate();
              this->allocator() = other.allocator();

              p_ = other.p_;
              len_ = other.len_;

              other.p_ = pointer();
              other.len_ = 0;
            } else {
              allocator_type a = allocator();
              pointer p = boost::allocator_allocate(a, other.len_);

              BOOST_TRY
              {
                boost::allocator_construct_n(
                  a, boost::to_address(p), other.len_, other.data());

                p_ = p;
                len_ = other.len_;
              }
              BOOST_CATCH(...)
              {
                boost::allocator_deallocate(a, p, other.len_);
                BOOST_RETHROW;
              }
              BOOST_CATCH_END
            }

            return *this;
          }

          size_type size() const BOOST_NOEXCEPT { return len_; }

          allocator_type& allocator() BOOST_NOEXCEPT
          {
            return boost::empty_value<allocator_type>::get();
          }

          allocator_type const& allocator() const BOOST_NOEXCEPT
          {
            return boost::empty_value<allocator_type>::get();
          }

          value_type& operator[](size_type idx) BOOST_NOEXCEPT
          {
            BOOST_ASSERT(idx < len_);
            return data()[idx];
          }

          value_type const& operator[](size_type idx) const BOOST_NOEXCEPT
          {
            BOOST_ASSERT(idx < len_);
            return data()[idx];
          }

          value_type& front() BOOST_NOEXCEPT { return this->operator[](0); }

          value_type& back() BOOST_NOEXCEPT
          {
            return this->operator[](len_ - 1);
          }

          value_type const& front() const BOOST_NOEXCEPT
          {
            return this->operator[](0);
          }

          value_type const& back() const BOOST_NOEXCEPT
          {
            return this->operator[](len_ - 1);
          }

          value_type* data() const BOOST_NOEXCEPT
          {
            return boost::to_address(p_);
          }

          void clear() BOOST_NOEXCEPT { deallocate(); }

        private:
          void deallocate() BOOST_NOEXCEPT
          {
            if (!p_) {
              BOOST_ASSERT(len_ == 0);
              return;
            }

            allocator_type a = allocator();
            boost::allocator_destroy_n(a, data(), len_);
            boost::allocator_deallocate(a, p_, len_);

            len_ = 0;
            p_ = pointer();
          }
        };

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
            return static_cast<boost::uint64_t>(
              ((__uint128_t)lowbits * d) >> 64);
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
          typedef typename boost::allocator_rebind<Allocator, node>::type
            node_allocator_type;
          typedef typename boost::allocator_pointer<node_allocator_type>::type
            pointer;

          pointer next;
          BOOST_ALIGNMENT(boost::alignment::alignment_of<value_type>::value)
          unsigned char buf[sizeof(value_type)];

          node() : next() {}

          value_type* value_ptr() BOOST_NOEXCEPT
          {
            return reinterpret_cast<value_type*>(buf);
          }

          value_type& value() BOOST_NOEXCEPT
          {
            return *reinterpret_cast<value_type*>(buf);
          }
        };

        template <class Allocator> struct bucket
        {
          typedef typename node<Allocator>::pointer node_pointer;

          node_pointer next;

          bucket() : next() {}
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

        template <class Bucket, class Allocator>
        struct grouped_bucket_iterator
            : public boost::iterator_facade<
                grouped_bucket_iterator<Bucket, Allocator>, Bucket,
                boost::forward_traversal_tag>
        {
        public:
          typedef typename boost::allocator_rebind<Allocator, Bucket>::type
            bucket_allocator_type;
          typedef typename boost::allocator_pointer<bucket_allocator_type>::type
            bucket_pointer;

          typedef typename boost::allocator_rebind<Allocator,
            bucket_group<Bucket, Allocator> >::type bucket_group_allocator_type;
          typedef
            typename boost::allocator_pointer<bucket_group_allocator_type>::type
              bucket_group_pointer;

        private:
          bucket_pointer p;
          bucket_group_pointer pbg;

        public:
          grouped_bucket_iterator() : p(), pbg() {}

        private:
          friend class boost::iterator_core_access;

          template <typename, typename, typename>
          friend class grouped_bucket_array;

          static const std::size_t N;

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
            std::size_t const offset =
              static_cast<std::size_t>(p - pbg->buckets);

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

        template <class Bucket, class Allocator>
        const std::size_t grouped_bucket_iterator<Bucket, Allocator>::N =
          bucket_group<Bucket, Allocator>::N;

        template <class Allocator> struct const_grouped_local_bucket_iterator;

        template <class Allocator>
        struct grouped_local_bucket_iterator
            : public boost::iterator_facade<
                grouped_local_bucket_iterator<Allocator>,
                typename boost::allocator_value_type<Allocator>::type,
                boost::forward_traversal_tag>
        {
          typedef typename node<Allocator>::pointer node_pointer;
          typedef typename node<Allocator>::value_type value_type;

        public:
          grouped_local_bucket_iterator() : p() {}

        private:
          friend class boost::iterator_core_access;

          template <typename, typename, typename>
          friend class grouped_bucket_array;

          template <class> friend struct const_grouped_local_bucket_iterator;

          grouped_local_bucket_iterator(node_pointer p_) : p(p_) {}

          value_type& dereference() const BOOST_NOEXCEPT { return p->value; }

          bool equal(
            const grouped_local_bucket_iterator& x) const BOOST_NOEXCEPT
          {
            return p == x.p;
          }

          void increment() BOOST_NOEXCEPT { p = p->next; }

          node_pointer p;
        };

        template <class Allocator>
        struct const_grouped_local_bucket_iterator
            : public boost::iterator_facade<
                const_grouped_local_bucket_iterator<Allocator>,
                typename boost::allocator_value_type<Allocator>::type const,
                boost::forward_traversal_tag>
        {
          typedef typename node<Allocator>::pointer node_pointer;
          typedef typename node<Allocator>::value_type const value_type;

        public:
          const_grouped_local_bucket_iterator() : p() {}
          const_grouped_local_bucket_iterator(
            grouped_local_bucket_iterator<Allocator> it)
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

        public:
          typedef node<Allocator> node_type;
          typedef typename boost::allocator_rebind<Allocator, node_type>::type
            node_allocator_type;
          typedef typename boost::allocator_pointer<node_allocator_type>::type
            node_pointer;

        private:
          typedef SizePolicy size_policy;

          typedef typename boost::allocator_rebind<Allocator, Bucket>::type
            bucket_allocator_type;
          typedef typename boost::allocator_pointer<bucket_allocator_type>::type
            bucket_pointer;
          typedef boost::pointer_traits<bucket_pointer> bucket_pointer_traits;

          typedef bucket_group<Bucket, Allocator> group;
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
          typedef grouped_bucket_iterator<Bucket, Allocator> iterator;
          typedef grouped_local_bucket_iterator<Allocator> local_iterator;
          typedef const_grouped_local_bucket_iterator<Allocator>
            const_local_iterator;

        private:
          static const std::size_t N = group::N;

          Allocator allocator;
          node_allocator_type node_allocator;

          std::size_t size_index_, size_;
          dynamic_array<Bucket, bucket_allocator_type> buckets;
          dynamic_array<group, group_allocator_type> groups;

        public:
          grouped_bucket_array(size_type n, const Allocator& al)
              : allocator(al), node_allocator(al),
                size_index_(size_policy::size_index(n)),
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

          grouped_bucket_array(
            BOOST_RV_REF(grouped_bucket_array) other) BOOST_NOEXCEPT
              : allocator(boost::move(other.allocator)),
                node_allocator(boost::move(other.node_allocator)),
                size_index_(other.size_index_),
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

            other.size_index_ = 0;
            other.size_ = 0;
            return *this;
          };

          Allocator get_allocator() const { return allocator; }

          node_allocator_type const& get_node_allocator() const
          {
            return node_allocator;
          }

          node_allocator_type& get_node_allocator() { return node_allocator; }

          void reset_allocator(Allocator const& allocator_)
          {
            allocator = allocator_;
            node_allocator = node_allocator_type(allocator);

            groups.allocator() = group_allocator_type(allocator);
            buckets.allocator() = bucket_allocator_type(allocator);
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
              pbg.p = bucket_pointer_traits::pointer_to(
                const_cast<Bucket&>(buckets.back()));
            }

            return pbg;
          }

          local_iterator begin(size_type n) const
          {
            return local_iterator(buckets[n].next);
          }

          local_iterator end(size_type) const { return local_iterator(); }

          size_type capacity() const BOOST_NOEXCEPT { return size_; }

          iterator at(size_type n) const
          {
            iterator pbg(bucket_pointer_traits::pointer_to(
                           const_cast<Bucket&>(buckets[n])),
              group_pointer_traits::pointer_to(
                const_cast<group&>(groups[static_cast<std::size_t>(n / N)])));

            return pbg;
          }

          span<Bucket> raw()
          {
            BOOST_ASSERT(size_ == 0 || size_ < buckets.size());
            return span<Bucket>(buckets.data(), size_);
          }

          size_type position(std::size_t hash) const
          {
            return size_policy::position(hash, size_index_);
          }

          void clear()
          {
            size_index_ = 0;
            size_ = 0;

            buckets.clear();
            groups.clear();
          }

          void insert_node(iterator itb, node_pointer p) BOOST_NOEXCEPT
          {
            if (!itb->next) { // empty bucket
              typename iterator::bucket_pointer pb = itb.p;
              typename iterator::bucket_group_pointer pbg = itb.pbg;

              std::size_t n =
                static_cast<std::size_t>(boost::to_address(pb) - &buckets[0]);
              if (!pbg->bitmask) { // empty group
                pbg->buckets =
                  bucket_pointer_traits::pointer_to(buckets[N * (n / N)]);
                pbg->next = groups.back().next;
                pbg->next->prev = pbg;
                pbg->prev = group_pointer_traits::pointer_to(groups.back());
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
            group_pointer pbg =
                            group_pointer_traits::pointer_to(groups.front()),
                          last =
                            group_pointer_traits::pointer_to(groups.back());

            for (; pbg != last; ++pbg) {
              for (std::size_t n = 0; n < N; ++n) {
                if (!pbg->buckets[static_cast<std::ptrdiff_t>(n)].next)
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
