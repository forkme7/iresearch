////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "filter_test_case_base.hpp"
#include "store/memory_directory.hpp"
#include "formats/formats_10.hpp"
#include "store/fs_directory.hpp"
#include "search/column_existence_filter.hpp"
#include "search/sort.hpp"

NS_BEGIN(tests)

class column_existence_filter_test_case
    : public filter_test_case_base {
 protected:
  void simple_sequential_exact_match() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // 'prefix' column
    {
      const std::string column_name = "prefix";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'name' column
    {
      const std::string column_name = "name";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'seq' column
    {
      const std::string column_name = "seq";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'same' column
    {
      const std::string column_name = "same";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      size_t docs_count = 0;
      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
        ++docs_count;
      }
      ASSERT_FALSE(column_it->next());
      ASSERT_EQ(segment.docs_count(), docs_count);
      ASSERT_EQ(segment.live_docs_count(), docs_count);
    }

    // 'value' column
    {
      const std::string column_name = "value";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'duplicated' column
    {
      const std::string column_name = "duplicated";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
      }
      ASSERT_FALSE(column_it->next());
    }

    // 'prefix' column
    {
      const std::string column_name = "prefix";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto column_it = segment.values_iterator(column_name);
      auto filter_it = prepared->execute(segment);

      while (filter_it->next()) {
        ASSERT_TRUE(column_it->next());
        ASSERT_EQ(filter_it->value(), column_it->value().first);
      }
      ASSERT_FALSE(column_it->next());
    }

    // invalid column
    {
      const std::string column_name = "invalid_column";

      irs::by_column_existence filter;
      filter.prefix_match(false);
      filter.field(column_name);

      auto prepared = filter.prepare(
        *rdr, irs::order::prepared::unordered()
      );

      ASSERT_EQ(1, rdr->size());
      auto& segment = (*rdr)[0];

      auto filter_it = prepared->execute(segment);

      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), filter_it->value());
      ASSERT_FALSE(filter_it->next());
    }
  }

  void simple_sequential_prefix_match() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // 'name' column
    {
//      const std::string column_prefix = "name";
//
//      irs::by_column_existence filter;
//      filter.prefix_match(true);
//      filter.field(column_name);
//
//      auto prepared = filter.prepare(
//        *rdr, irs::order::prepared::unordered()
//      );
//
//      ASSERT_EQ(1, rdr->size());
//      auto& segment = (*rdr)[0];
//
//      auto filter_it = prepared->execute(segment);
//
//      while (filter_it->next()) {
//        ASSERT_TRUE(column_it->next());
//        ASSERT_EQ(filter_it->value(), column_it->value().first);
//      }
//      ASSERT_FALSE(column_it->next());
    }
  }
}; // column_existence_filter_test_case

NS_END

TEST(by_column_existence, ctor) {
  irs::by_column_existence filter;
  ASSERT_EQ(irs::by_column_existence::type(), filter.type());
  ASSERT_EQ(false, filter.prefix_match());
  ASSERT_TRUE(filter.field().empty());
  ASSERT_EQ(irs::boost::no_boost(), filter.boost());
}

TEST(by_column_existence, boost) {
  // FIXME
}

TEST(by_column_existence, equal) {
  ASSERT_EQ(ir::by_column_existence(), ir::by_column_existence());

  {
    irs::by_column_existence q0;
    q0.field("name");

    irs::by_column_existence q1;
    q1.field("name");
    ASSERT_EQ(q0, q1);
    ASSERT_EQ(q0.hash(), q1.hash());
  }

  {
    irs::by_column_existence q0;
    q0.field("name");
    q0.prefix_match(true);

    irs::by_column_existence q1;
    q1.field("name");
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_column_existence q0;
    q0.field("name");
    q0.prefix_match(true);

    irs::by_column_existence q1;
    q1.field("name1");
    q1.prefix_match(true);
    ASSERT_NE(q0, q1);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_column_existence_filter_test_case
    : public tests::column_existence_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    static irs::version10::format FORMAT;
    return irs::format::ptr(&FORMAT, [](irs::format*)->void{});
  }
};

TEST_F(memory_column_existence_filter_test_case, by_column_existence) {
  simple_sequential_exact_match();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_column_existence_test_case
    : public tests::column_existence_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    const fs::path dir = fs::path(test_dir()).append("index");
    return new irs::fs_directory(dir.string());
  }

  virtual irs::format::ptr get_codec() override {
    static irs::version10::format FORMAT;
    return irs::format::ptr(&FORMAT, [](irs::format*)->void{});
  }
};

TEST_F(fs_column_existence_test_case, by_column_existence) {
  simple_sequential_exact_match();
}
