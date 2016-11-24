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

#include "bm25.hpp"

#include "scorers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/field_meta.hpp"

NS_ROOT

// bm25 similarity
// bm25(doc, term) = idf(term) * ((k + 1) * tf(doc, term)) / (k * (1.0 - b + b * |doc|/avgDL) + tf(doc, term))

// inverted document frequency
// idf(term) = 1 + log(total_docs_count / (1 + docs_count(term)) 

// term frequency
// tf(doc, term) = sqrt(frequency(doc, term));

// document length
// Current implementation is using the following as the document length
// |doc| = index_time_doc_boost / sqrt(# of terms in a field within a document)

// average document length
// avgDL = total_term_frequency / total_docs_count;

NS_BEGIN(bm25)

// empty frequency
const frequency EMPTY_FREQ;

// set of features required for bm25 model
const flags FEATURES{ frequency::type(), norm::type() };

struct stats final : attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();

  stats() : attribute(stats::type()) { }

  virtual void clear() { 
    idf = 1.f;
    norm_const = 1.f;
    norm_length = 0.f;
  }
  
  float_t idf; // precomputed idf value
  float_t norm_const; // precomputed k*(1-b)
  float_t norm_length; // precomputed k*b/avgD
}; // stats

DEFINE_ATTRIBUTE_TYPE(iresearch::bm25::stats);
DEFINE_FACTORY_DEFAULT(stats);

typedef bm25_sort::score_t score_t;

class scorer : public iresearch::sort::scorer_base<bm25::score_t> {
 public:
  DECLARE_FACTORY(scorer);

  scorer(
      float_t k, 
      iresearch::boost::boost_t boost,
      const bm25::stats* stats,
      const frequency* freq)
    : freq_(freq ? freq : &EMPTY_FREQ),
      num_(boost * (k + 1) * stats->idf),
      norm_const_(k) {
    assert(freq_);
  }

  virtual void score(score_t& score_buf) override {
    const float_t freq = tf();
    score_buf = num_ * freq / (norm_const_ + freq);
  }

 protected:
  FORCE_INLINE float_t tf() const {
    return float_t(std::sqrt(freq_->value));
  };

  const frequency* freq_; // document frequency
  float_t num_; // partially precomputed numerator : boost * (k + 1) * idf
  float_t norm_const_; // 'k' factor
}; // scorer

class norm_scorer final : public scorer {
 public:
  DECLARE_FACTORY(norm_scorer);

  norm_scorer(
      float_t k, 
      iresearch::boost::boost_t boost,
      const bm25::stats* stats,
      const frequency* freq,
      const iresearch::norm* norm)
    : scorer(k, boost, stats, freq),
      norm_(norm) {
    assert(norm_);

    // if there is no norms, assume that b==0
    if (!norm_->empty()) {
      norm_const_ = stats->norm_const;
      norm_length_ = stats->norm_length;
    }
  }

  virtual void score(score_t& score_buf) override {
    const float_t freq = tf();
    score_buf = num_ * freq / (norm_const_ + norm_length_ * norm_->read() + freq);
  }

 private:
  const iresearch::norm* norm_;
  float_t norm_length_{ 0.f }; // precomputed 'k*b/avgD' if norms presetn, '0' otherwise
}; // norm_scorer

class collector final : public iresearch::sort::collector {
 public:
  explicit collector(float_t k, float_t b) 
    : k_(k), b_(b) {
  }

  virtual void collect(
      const sub_reader& /* sub_reader */,
      const term_reader& term_reader,
      const attributes& term_attrs) {
    const iresearch::term_meta* meta = term_attrs.get<iresearch::term_meta>();
    if (meta) {
      docs_count += meta->docs_count;
    }
    total_term_freq += term_reader.size(); // TODO: need term_freq here
  }

  virtual void after_collect(
      const iresearch::index_reader& index_reader, 
      iresearch::attributes& query_attrs) override {
    stats* bm25stats = query_attrs.add<stats>();

    const auto total_docs_count = index_reader.docs_count();
    assert(total_docs_count);
    const auto avg_doc_len = 0 == total_term_freq 
      ? 1.f
      : static_cast<float_t>(total_term_freq) / total_docs_count;

    // precomputed idf value
    bm25stats->idf = 1 + static_cast<float_t>(
      std::log(total_docs_count / double_t(docs_count + 1))
    ); 

    // precomputed length norm
    const float_t kb = k_ * b_;
    bm25stats->norm_const = k_ - kb;
    bm25stats->norm_length = kb / avg_doc_len ;     
    
    // add norm attribute
    query_attrs.add<norm>();
  }

 private:
  uint64_t docs_count = 0; // document frequency
  uint64_t total_term_freq = 0;
  float_t k_;
  float_t b_;
}; // collector

class sort final : iresearch::sort::prepared_base<bm25::score_t> {
 public:
  DECLARE_FACTORY(prepared);

  sort(float_t k, float_t b, bool reverse)
    : k_(k), b_(b) {
    static const std::function<bool(score_t, score_t)> greater = std::greater<score_t>();
    static const std::function<bool(score_t, score_t)> less = std::less<score_t>();
    less_ = reverse ? &greater : &less;
  }

  virtual const flags& features() const {
    return bm25::FEATURES;
  }

  virtual iresearch::sort::collector::ptr prepare_collector() const {
    return iresearch::sort::collector::make<bm25::collector>(k_, b_);
  }

  virtual scorer::ptr prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const attributes& query_attrs, 
      const attributes& doc_attrs) const override {

    iresearch::norm* norm = query_attrs.get<iresearch::norm>();
    if (norm && norm->reset(segment, field.meta().norm, *doc_attrs.get<document>())) {
      return bm25::scorer::make<bm25::norm_scorer>(      
        k_, 
        boost::extract(query_attrs),
        query_attrs.get<bm25::stats>(),
        doc_attrs.get<frequency>(),
        norm
      );
    }

    return bm25::scorer::make<bm25::scorer>(      
      k_, 
      boost::extract(query_attrs),
      query_attrs.get<bm25::stats>(),
      doc_attrs.get<frequency>()
    );
  }

  virtual void add(score_t& dst, const score_t& src) const override {
    dst += src;
  }

  virtual bool less(const score_t& lhs, const score_t& rhs) const override {
    return (*less_)(lhs, rhs);
  }

 private: 
  const std::function<bool(score_t, score_t)>* less_;
  float_t k_;
  float_t b_;
}; // sort

NS_END // bm25 

DEFINE_SORT_TYPE_NAMED(iresearch::bm25_sort, "bm25");
REGISTER_SCORER(iresearch::bm25_sort);
DEFINE_FACTORY_SINGLETON(bm25_sort);

bm25_sort::bm25_sort(float_t k, float_t b) 
  : sort(bm25_sort::type()), k_(k), b_(b) {
  reverse(true); // return the most relevant results first
}

sort::prepared::ptr bm25_sort::prepare() const {
  return bm25::sort::make<bm25::sort>(k_, b_, reverse());
}

NS_END // ROOT
