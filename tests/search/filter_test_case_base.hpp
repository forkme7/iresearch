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

#ifndef IRESEARCH_FILTER_TEST_CASE_BASE
#define IRESEARCH_FILTER_TEST_CASE_BASE

#include "tests_shared.hpp"
#include "analysis/token_attributes.hpp"
#include "search/cost.hpp"
#include "search/filter.hpp"
#include "search/tfidf.hpp"
#include "utils/singleton.hpp"
#include "utils/type_limits.hpp"
#include "index/index_tests.hpp"

namespace ir = iresearch;

namespace tests {
namespace sort {

////////////////////////////////////////////////////////////////////////////////
/// @class boost
/// @brief boost scorer assign boost value to the particular document score
////////////////////////////////////////////////////////////////////////////////
struct boost : public iresearch::sort {
  class scorer: public iresearch::sort::scorer_base<iresearch::boost::boost_t> {
   public:
    DECLARE_FACTORY(scorer);

    scorer(
      const iresearch::attribute_ref<iresearch::boost>& boost,
      const iresearch::attributes& attrs
    ): attrs_(attrs), boost_(boost) {
    }

    virtual void score(score_t& score) {
      score = boost_ ? boost_->value : 0;
    }

   private:
    const iresearch::attributes& attrs_;
    const iresearch::attribute_ref<iresearch::boost>& boost_;
  }; // sort::boost::scorer

  class prepared: public iresearch::sort::prepared_base<iresearch::boost::boost_t> {
   public:
    DECLARE_FACTORY(prepared);
    prepared(bool reverse):
      iresearch::sort::prepared_base<iresearch::boost::boost_t>(iresearch::flags{}) {
      static const std::function<bool(score_t, score_t)> greater = std::greater<score_t>();
      static const std::function<bool(score_t, score_t)> less = std::less<score_t>();
      less_ = reverse ? &greater : &less;
    }

    virtual collector::ptr prepare_collector() const override {
      return nullptr; // do not need to collect stats
    }

    virtual scorer::ptr prepare_scorer(
        const iresearch::sub_reader&,
        const iresearch::term_reader&,
        const iresearch::attributes& query_attrs, 
        const iresearch::attributes& doc_attrs) const override {
      return boost::scorer::make<boost::scorer>(
        query_attrs.get<iresearch::boost>(), query_attrs
       );
    }

    virtual void add(score_t& dst, const score_t& src) const override {
      dst += src;
    }

    virtual bool less(const score_t& lhs, const score_t& rhs) const override {
      return (*less_)(lhs, rhs);
    }

   private:
    const iresearch::boost* boost;
    const std::function<bool(score_t, score_t)>* less_;
  }; // sort::boost::prepared

  DECLARE_SORT_TYPE();
  DECLARE_FACTORY_DEFAULT();
  typedef iresearch::boost::boost_t score_t;
  boost() : sort(boost::type()) {}
  virtual sort::prepared::ptr prepare() const {
    return boost::prepared::make<boost::prepared>(reverse());
  }
}; // sort::boost

//////////////////////////////////////////////////////////////////////////////
/// @brief order by frequency, then if equal order by doc_id_t
//////////////////////////////////////////////////////////////////////////////
struct frequency_sort: public iresearch::sort {
  DECLARE_SORT_TYPE();

  struct score_t {
    iresearch::doc_id_t id;
    double value;
  };

  class prepared: public iresearch::sort::prepared_base<score_t> {
   public:
    struct count: public iresearch::basic_attribute<size_t> {
      size_t value;
      count(): iresearch::basic_attribute<size_t>(count::type()), value(0) {}
      DECLARE_ATTRIBUTE_TYPE();
      DECLARE_FACTORY_DEFAULT();
    };

    class collector: public iresearch::sort::collector {
     public:
      virtual void collect(
          const iresearch::sub_reader& sub_reader, 
          const iresearch::term_reader& term_reader,
          const iresearch::attributes& term_attrs
      ) override {
        meta_attr = term_attrs.get<iresearch::term_meta>();
        docs_count += meta_attr->docs_count;
      }

      virtual void after_collect(
        const iresearch::index_reader& idx_reader, iresearch::attributes& query_attrs
      ) override {
        query_attrs.add<count>()->value = docs_count;
        docs_count = 0;
      }

     private:
      size_t docs_count{};
      iresearch::attribute_ref<iresearch::term_meta> meta_attr;
    };

    class scorer: public iresearch::sort::scorer_base<score_t> {
     public:
      scorer(const size_t* v_docs_count, const iresearch::attribute_ref<iresearch::document>& doc_id_t):
        doc_id_t_attr(doc_id_t), docs_count(v_docs_count) {
      }

      virtual void score(score_t& score_buf) override {
        score_buf.id = doc_id_t_attr->value;
        score_buf.value = 1. / *docs_count;
      }

     private:
      const iresearch::attribute_ref<iresearch::document>& doc_id_t_attr;
      const size_t* docs_count;
    };

    DECLARE_FACTORY(prepared);

    prepared(): iresearch::sort::prepared_base<score_t>(iresearch::flags{}) {
    }

    virtual collector::ptr prepare_collector() const override {
      return iresearch::sort::collector::make<frequency_sort::prepared::collector>();
    }

    virtual scorer::ptr prepare_scorer(
        const iresearch::sub_reader&,
        const iresearch::term_reader&,
        const iresearch::attributes& query_attrs, 
        const iresearch::attributes& doc_attrs) const override {
      auto& doc_id_t = doc_attrs.get<iresearch::document>();
      auto& count = query_attrs.get<prepared::count>();
      const size_t* docs_count = count ? &(count->value) : nullptr;
      return sort::scorer::make<frequency_sort::prepared::scorer>(docs_count, doc_id_t);
    }

    virtual void prepare_score(score_t& score) const {
      score.id = ir::type_limits<ir::type_t::doc_id_t>::invalid();
      score.value = std::numeric_limits<double>::infinity();
    }

    virtual void add(score_t& dst, const score_t& src) const override {
      if (std::numeric_limits<double>::infinity()) {
        dst.id = src.id;
        dst.value = 0;
      }

      dst.value += src.value;
    }

    virtual bool less(const score_t& lhs, const score_t& rhs) const override {
      return lhs.value == rhs.value
        ? std::less<iresearch::doc_id_t>()(lhs.id, rhs.id)
        : std::less<double>()(lhs.value, rhs.value);
    }
  };

  DECLARE_FACTORY_DEFAULT();
  frequency_sort(): sort(frequency_sort::type()) {}
  virtual prepared::ptr prepare() const {
    return frequency_sort::prepared::make<frequency_sort::prepared>();
  }
}; // sort::frequency_sort

} // sort

class filter_test_case_base : public index_test_base {
 protected:
  typedef std::vector<iresearch::doc_id_t> docs_t;
  typedef std::vector<ir::cost::cost_t> costs_t;

  void check_query(
      const ir::filter& filter,
      const std::vector<iresearch::doc_id_t>& expected,
      const std::vector<ir::cost::cost_t>& expected_costs, 
      const ir::index_reader& rdr) {
    std::vector<ir::doc_id_t> result;
    std::vector<ir::cost::cost_t> result_costs;
    get_query_result(
      filter.prepare(rdr, iresearch::order::prepared::unordered()),
      expected, rdr, 
      result, result_costs);
    ASSERT_EQ(expected, result);
    ASSERT_EQ(expected_costs, result_costs);
  }

  void check_query(
      const ir::filter& filter,
      const std::vector<iresearch::doc_id_t>& expected,
      const ir::index_reader& rdr) {
    std::vector<ir::doc_id_t> result;
    std::vector<ir::cost::cost_t> result_costs;
    get_query_result(
      filter.prepare(rdr, iresearch::order::prepared::unordered()),
      expected, rdr, 
      result, result_costs);
    ASSERT_EQ(expected, result);
  }

  void check_query(
      const ir::filter& filter,
      const iresearch::order& order,
      const std::vector<iresearch::doc_id_t>& expected,
      const ir::index_reader& rdr) {
    typedef std::pair<iresearch::string_ref, iresearch::doc_id_t> result_item_t;
    auto prepared_order = order.prepare();
    auto prepared_filter = filter.prepare(rdr, prepared_order);
    auto score_less = [&prepared_order](
      const iresearch::bytes_ref& lhs, const iresearch::bytes_ref& rhs
    )->bool {
      return prepared_order.less(lhs.c_str(), rhs.c_str());
    };
    std::multimap<iresearch::bstring, iresearch::doc_id_t, decltype(score_less)> scored_result(score_less);

    for (const auto& sub: rdr) {
      auto docs = prepared_filter->execute(sub, prepared_order);
      auto& score = docs->attributes().get<ir::score>();

      while(docs->next()) {
        docs->score();
        ASSERT_TRUE(score);
        scored_result.emplace(score->value(), docs->value());
      }
    }

    std::vector<iresearch::doc_id_t> result;

    for (auto& entry: scored_result) {
      result.emplace_back(entry.second);
    }

    ASSERT_EQ(expected, result);
  }

 private:
  void get_query_result(
      const iresearch::filter::prepared::ptr& q,
      const std::vector<iresearch::doc_id_t>& expected,
      const ir::index_reader& rdr,
      std::vector<ir::doc_id_t>& result,
      std::vector<ir::cost::cost_t>& result_costs) {
    for (const auto& sub : rdr) {
      auto docs = q->execute(sub); 
      result_costs.push_back(ir::cost::extract(docs->attributes()));
      for (;docs->next();) {
        docs->score();
        /* put score attributes to iterator */
        result.push_back(docs->value());
      }
    }
  }
};

struct empty_index_reader : iresearch::singleton<empty_index_reader>, iresearch::index_reader {
  virtual bool document(
      iresearch::doc_id_t, 
      const iresearch::stored_fields_reader::visitor_f&) const {
    return false; 
  }

  virtual bool document(
      iresearch::doc_id_t, 
      const iresearch::index_reader::document_visitor_f&) const {
    return false;
  }

  virtual uint64_t docs_count() const { return 0; }

  virtual uint64_t docs_count(const iresearch::string_ref& field) const { return 0; }

  virtual uint64_t docs_max() const { return 0; }

  virtual reader_iterator begin() const { return reader_iterator(); }

  virtual reader_iterator end() const { return reader_iterator(); }

  virtual size_t size() const { return 0; }
}; // index_reader

struct empty_sub_reader : iresearch::singleton<empty_sub_reader>, iresearch::sub_reader {
  struct empty_docs_iterator: docs_iterator_t {
    virtual bool next() { return false; }
    virtual iresearch::doc_id_t value() const { 
      return iresearch::type_limits<iresearch::type_t::doc_id_t>::invalid();
    }
  };

  struct empty_string_iterator : iresearch::iterator<const iresearch::string_ref&> {
    virtual const iresearch::string_ref& value() const {
      return iresearch::string_ref::nil;
    }

    virtual bool next() {
      return false;
    }
  };

  virtual bool document(
      iresearch::doc_id_t, 
      const iresearch::stored_fields_reader::visitor_f&) const { 
    return false; 
  }

  virtual bool document(
      iresearch::doc_id_t, 
      const iresearch::index_reader::document_visitor_f&) const { 
    return false; 
  }

  virtual const iresearch::columns_meta& columns() const {
    static iresearch::columns_meta empty;
    return empty;
  }
  
  virtual bool column(
      iresearch::field_id field,
      const iresearch::columnstore_reader::raw_reader_f& reader) const {
    return false;
  }
  
  virtual value_visitor_f values(
      iresearch::field_id field,
      const iresearch::columnstore_reader::value_reader_f& reader) const {
    return [] (iresearch::doc_id_t) { return false; };
  }

  virtual value_visitor_f values(
      const iresearch::string_ref& field, 
      const iresearch::columnstore_reader::value_reader_f& reader) const {
    return [] (iresearch::doc_id_t) { return false; };
  }

  virtual uint64_t docs_count() const { return 0; }

  virtual uint64_t docs_count(const iresearch::string_ref&) const { return 0; }
  
  virtual docs_iterator_t::ptr docs_iterator() const override { 
    return docs_iterator_t::make<empty_docs_iterator>(); 
  }
  
  virtual uint64_t docs_max() const { return 0; }

  virtual reader_iterator begin() const { return reader_iterator(); }

  virtual reader_iterator end() const { return reader_iterator(); }

  virtual iresearch::iterator<const iresearch::string_ref&>::ptr iterator() const {
    return iresearch::iterator<const iresearch::string_ref&>::make<empty_string_iterator>();
  }

  virtual const iresearch::term_reader* terms(const iresearch::string_ref&) const {
    return nullptr;
  }

  virtual size_t size() const { return 0; }

  virtual const iresearch::fields_meta& fields() const {
    static iresearch::fields_meta empty;
    return empty;
  }
}; // empty_sub_reader

struct empty_term_reader : iresearch::singleton<empty_term_reader>, iresearch::term_reader {
  virtual iresearch::seek_term_iterator::ptr iterator() const { return nullptr; }
  virtual const iresearch::field_meta& field() const { 
    static iresearch::field_meta EMPTY;
    return EMPTY;
  }

  // total number of terms
  virtual size_t size() const { return 0; }

  // total number of documents
  virtual uint64_t docs_count() const { return 0; }

  // less significant term
  virtual const iresearch::bytes_ref& (min)() const { return iresearch::bytes_ref::nil; }

  // most significant term
  virtual const iresearch::bytes_ref& (max)() const { return iresearch::bytes_ref::nil; }
}; // empty_term_reader

} // tests

#endif // IRESEARCH_FILTER_TEST_CASE_BASE
