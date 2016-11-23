//
// IResearch search engine 
// 
// Copyright � 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "sort.hpp"

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                       Attributes
// ----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(iresearch::boost);
DEFINE_FACTORY_DEFAULT(boost);

boost::boost()
  : basic_attribute<boost::boost_t>(boost::type(), boost_t(boost::no_boost())) {
}

// ----------------------------------------------------------------------------
// --SECTION--                                                             sort
// ----------------------------------------------------------------------------

sort::sort(const type_id& type) : type_(&type), rev_(false) { }

sort::~sort() { }

sort::collector::~collector() { }

sort::scorer::~scorer() { }

sort::prepared::~prepared() { }

// ----------------------------------------------------------------------------
// --SECTION--                                                            order 
// ----------------------------------------------------------------------------

const order& order::unordered() {
  static order ord;
  return ord;
}

void order::remove( const type_id& type ) {
  order_.erase( 
    std::remove_if( 
      order_.begin(), order_.end(),
      [type] ( const sort::ptr& s ) { return type == s->type(); }
  ));
}

const order::prepared& order::prepared::unordered() {
  static order::prepared ord;
  return ord;
}

order::prepared::prepared(order::prepared&& rhs)
  : order_(std::move(rhs.order_)),
    features_(std::move(rhs.features_)),
    size_(rhs.size_) {
  rhs.size_ = 0;
}

order::prepared& order::prepared::operator=(order::prepared&& rhs) {
  if (this != &rhs) {
    order_ = std::move(rhs.order_);
    features_ = std::move(rhs.features_);
    size_ = rhs.size_;
    rhs.size_ = 0;
  }
  return *this;
}

order::prepared order::prepare() const {
  order::prepared pord;
  pord.order_.reserve(order_.size());

  size_t offset = 0;
  std::for_each(
    begin(), end(),
    [&pord, &offset] (const sort& s) {
      pord.order_.emplace_back(s.prepare()); // strong exception guarantee
      prepared::prepared_sort& psort = pord.order_.back();
      const sort::prepared& bucket = *psort.bucket;
      pord.features_ |= bucket.features();
      psort.offset = offset;
      pord.size_ += bucket.size();
      offset += bucket.size();
  });

  return pord;
}

// ----------------------------------------------------------------------------
// --SECTION--                                                            stats 
// ----------------------------------------------------------------------------
order::prepared::stats::stats(collectors_t&& colls)
  : colls_(std::move(colls)) {
}

order::prepared::stats::stats(stats&& rhs)
  : colls_(std::move(rhs.colls_)) {
}

order::prepared::stats&
order::prepared::stats::operator=(stats&& rhs) {
  if (this != &rhs) {
    colls_ = std::move(rhs.colls_);
  }
  return *this;
}

void order::prepared::stats::collect(
    const sub_reader& segment_reader,
    const term_reader& term_reader,
    const attributes& term_attributes) const {
  /* collect */
  std::for_each(
    colls_.begin(), colls_.end(),
    [&segment_reader, &term_reader, &term_attributes](const sort::collector::ptr& col) {
      col->collect(segment_reader, term_reader, term_attributes);
  });
}

void order::prepared::stats::after_collect(
    const index_reader& index_reader,
    attributes& query_context) const {
  /* after collect */
  std::for_each(
    colls_.begin(), colls_.end(),
    [&index_reader,&query_context](const sort::collector::ptr& col) {
      col->after_collect(index_reader, query_context);
  });
}

// ----------------------------------------------------------------------------
// --SECTION--                                                          scorers
// ----------------------------------------------------------------------------

order::prepared::scorers::scorers(scorers_t&& scorers)
  : scorers_(std::move(scorers)) {
}

order::prepared::scorers::scorers(order::prepared::scorers&& rhs)
  : scorers_(std::move(rhs.scorers_)) {
}

order::prepared::scorers&
order::prepared::scorers::operator=(order::prepared::scorers&& rhs) {
  if (this != &rhs) {
    scorers_ = std::move(rhs.scorers_);
  }
  return *this;
}

void order::prepared::scorers::score(
  const order::prepared& ord, byte_type* scr
) const {
  size_t i = 0;
  std::for_each(
    scorers_.begin(), scorers_.end(),
    [this, &ord, &scr, &i] (const sort::scorer::ptr& scorer) {
      if (scorer) scorer->score(scr);
      const sort::prepared& bucket = *ord[i++].bucket;
      scr += bucket.size();
  });
}

order::prepared::prepared() : size_(0) { }

order::prepared::stats 
order::prepared::prepare_stats() const {
  /* initialize collectors */
  stats::collectors_t colls;
  colls.reserve(size());
  for_each([&colls](const prepared_sort& ps) {
    sort::collector::ptr collector = ps.bucket->prepare_collector();
    if (collector) {
      colls.emplace_back(std::move(collector));
    }
  });
  return prepared::stats(std::move(colls));
}

order::prepared::scorers 
order::prepared::prepare_scorers(
    const sub_reader& segment,
    const term_reader& field,
    const attributes& stats,
    const attributes& doc) const {
  scorers::scorers_t scrs;
  scrs.reserve(size());
  for_each([&segment, &field, &stats, &doc, &scrs] (const order::prepared::prepared_sort& ps) {
    const sort::prepared& bucket = *ps.bucket;
    scrs.emplace_back(bucket.prepare_scorer(segment, field, stats, doc));
  });
  return prepared::scorers(std::move(scrs));
}

bool order::prepared::less(const byte_type* lhs, const byte_type* rhs) const {
  if (!lhs) {
    return rhs != nullptr; // lhs(nullptr) == rhs(nullptr)
  }

  if (!rhs) {
    return true; // nullptr last
  }

  for (auto& prepared_sort: order_) {
    auto& bucket = *(prepared_sort.bucket);

    if (bucket.less(lhs, rhs)) {
      return true;
    }

    if (bucket.less(rhs, lhs)) {
      return false;
    }

    lhs += bucket.size();
    rhs += bucket.size();
  }

  return false;
}

void order::prepared::add(byte_type* lhs, const byte_type* rhs) const {
  for_each([&lhs, &rhs] (const prepared_sort& ps) {
    const sort::prepared& bucket = *ps.bucket;
    bucket.add(lhs, rhs);
    lhs += bucket.size();
    rhs += bucket.size();
  });
}

order::order(order&& rhs)
  : order_(std::move(rhs.order_)) {
}

order& order::operator=(order&& rhs) {
  if (this != &rhs) {
    order_ = std::move(rhs.order_);
  }
  return *this;
}

NS_END
