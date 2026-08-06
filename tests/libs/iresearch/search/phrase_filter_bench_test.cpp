#include "filter_test_case_base.hpp"
#include "formats/column/test_cs_helpers.hpp"
#include "iresearch/analysis/token_attributes.hpp"
#include "iresearch/index/iterators.hpp"
#include "iresearch/search/boolean_filter.hpp"
#include "iresearch/search/multiterm_query.hpp"
#include "iresearch/search/phrase_filter.hpp"
#include "iresearch/search/phrase_query.hpp"
#include "iresearch/search/term_query.hpp"
#include "tests_shared.hpp"

struct Config {
    inline static const std::string repeat_pattern_json = "interval_bench_repeat.json";
    inline static const std::string freqs_discrete_json = "interval_bench_freqs_discrete.json";
    inline static const std::string freqs_equal_json = "interval_bench_freqs_equal.json";
    inline static const std::string big_data_repeat_pattern_json = "interval_bench_big_data_repeat.json";
    inline static const std::string big_data_freqs_discrete_json = "interval_bench_big_data_freqs_discrete.json";
    inline static const std::string big_data_freqs_equal_json = "interval_bench_big_data_freqs_equal.json";
};

void print_time(std::ostream& out, std::chrono::microseconds duration) {
    if (duration.count() < 1000) {
        out << duration.count() << " microseconds" << std::endl;
    } else if (duration.count() < 1000000) {
        out << duration.count() / 1000.0 << " milliseconds" << std::endl;
    } else {
        out << duration.count() / 1000000.0 << " seconds" << std::endl;
    }
}

template <typename Func>
void timeEstimateSubCall(const std::string& test_name, Func func, std::ostream& out) {
    auto start = std::chrono::high_resolution_clock::now();
    func(test_name, out);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    out << "Test '" << test_name << "' completed in: ";
    print_time(out, duration);
}

template <typename Func>
void timeEstimate(const std::string& test_name, Func func) {
    std::fstream out(test_name + "_results.txt", std::ios::out);
    out << "=== Starting test: " << test_name << " ===" << std::endl;
    out << std::fixed << std::setprecision(3);
    auto start = std::chrono::high_resolution_clock::now();

    func(test_name, out);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    out << "All tests '" << test_name << "' completed in: ";
    print_time(out, duration);
    
    out << "=== Finished: " << test_name << " ===" << std::endl;
}


class PhraseFilterBenchTestCase : public tests::FilterTestCaseBase {
    inline static constexpr irs::field_id kName = 1;
    auto StoreName() {
      return [](irs::IndexWriter::Document& doc, const tests::Document& src) {
        const auto* name =
          dynamic_cast<const tests::StringField*>(src.stored.get("name"));
        if (name) {
          irs::tests::StoreFieldAt(*doc.Columnstore(), kName, doc.DocId(), *name);
        }
      };
    }

    static void AnalyzedJsonFieldFactory(tests::Document& doc, const std::string& name,
                                  const tests::JsonDocGenerator::JsonValue& data) {
      typedef tests::TextField<std::string> TextField;

      class StringField : public tests::StringField {
      public:
        StringField(const std::string& name, const std::string_view& value)
          : tests::StringField(name, value, irs::IndexFeatures::Freq) {}
      };

      if (data.is_string()) {
        // analyzed field
        doc.indexed.push_back(
          std::make_shared<TextField>(std::string(name.data()) + "_anl", data.str));

        // not analyzed field
        doc.insert(std::make_shared<StringField>(name, data.str));
      }
    }


public:
    void estimateTimePhraseFilter(const std::string& input) {
      {
        tests::JsonDocGenerator gen(resource(input),
                                    &AnalyzedJsonFieldFactory);
        add_segment(gen, irs::kOmCreate, irs::tests::DefaultWriterOptions(),
                    StoreName());
      }

      auto rdr = open_reader(irs::tests::DefaultReaderOptions());

      auto search_test = [&rdr](const std::string& test_name, std::ostream& out) {
          irs::ByPhrase q;
          *q.mutable_field() = "phrase_anl";
          q.mutable_options()->push_back<irs::ByTermOptions>().term =
              irs::ViewCast<irs::byte_type>(std::string_view("fox"));
          q.mutable_options()->push_back<irs::ByTermOptions>(1, 3).term =
              irs::ViewCast<irs::byte_type>(std::string_view("quick"));
          q.mutable_options()->push_back<irs::ByTermOptions>(1, 3).term =
              irs::ViewCast<irs::byte_type>(std::string_view("brown"));
          q.mutable_options()->push_back<irs::ByTermOptions>(1, 3).term =
              irs::ViewCast<irs::byte_type>(std::string_view("jumps"));

          auto prepared = q.prepare({.index = rdr});

          auto sub = rdr.begin();
          auto docs = prepared->execute({.segment = *sub});
          auto docs_seek = prepared->execute({.segment = *sub});
          docs->next();
          out << "Test search results:\n";
          while (!irs::doc_limits::eof(docs->value())) {
              out << docs->value() << '\n';
              docs->next();
          }
      };

      auto freqs_test = [&rdr](const std::string& test_name, std::ostream& out) {
          irs::ByPhrase q;
          *q.mutable_field() = "phrase_anl";
          q.mutable_options()->push_back<irs::ByTermOptions>().term =
              irs::ViewCast<irs::byte_type>(std::string_view("fox"));
          q.mutable_options()->push_back<irs::ByTermOptions>(1, 3).term =
              irs::ViewCast<irs::byte_type>(std::string_view("quick"));
          q.mutable_options()->push_back<irs::ByTermOptions>(1, 3).term =
              irs::ViewCast<irs::byte_type>(std::string_view("brown"));
          q.mutable_options()->push_back<irs::ByTermOptions>(1, 3).term =
              irs::ViewCast<irs::byte_type>(std::string_view("jumps"));

          tests::sort::CustomSort sort;
          irs::DocIterator* it = nullptr;
          sort.scorer_score = [&](const irs::ScoreOperator*, irs::score_t* score,
                                  size_t n) {
            *score = it->value();
          };

          auto prepared = q.prepare({
            .index = rdr,
            .scorer = &sort,
          });
          auto sub = rdr.begin();
          auto docs = prepared->execute({.segment = *sub});
          auto docs_seek = prepared->execute({
            .segment = *sub,
            .scorer = &sort,
          });
          tests::sort::FrequencyScore freq_score;
          auto* freq_seek = irs::get<irs::FreqBlockAttr>(*docs_seek);
          docs->next();
          out << "Test results:\n";
          while (!irs::doc_limits::eof(docs->value())) {
              docs_seek->seek(docs->value());
              docs_seek->FetchScoreArgs(0);
              out << freq_seek->value[0] << '\n';
              docs->next();
          }
      };
      auto tests = [&](const std::string& test_name, std::ostream& out) {
        timeEstimateSubCall("single_search", search_test, out);
        timeEstimateSubCall("freqs_search", freqs_test, out);
      };
      auto params = GetParam();
      std::string test_name = PhraseFilterBenchTestCase::to_string(
        ::testing::TestParamInfo<decltype(params)>(params, 0)
      );
      timeEstimate(input + "__" + test_name, tests);
  }
};

TEST_P(PhraseFilterBenchTestCase, repeated_phrase) {
  estimateTimePhraseFilter(Config::repeat_pattern_json);
}

TEST_P(PhraseFilterBenchTestCase, freqs_equal) {
  estimateTimePhraseFilter(Config::freqs_equal_json);
}

TEST_P(PhraseFilterBenchTestCase, discrete_freqs) {
  estimateTimePhraseFilter(Config::freqs_discrete_json);
}

TEST_P(PhraseFilterBenchTestCase, big_data_repeated_phrase) {
  estimateTimePhraseFilter(Config::big_data_repeat_pattern_json);
}

TEST_P(PhraseFilterBenchTestCase, big_data_freqs_equal) {
  estimateTimePhraseFilter(Config::big_data_freqs_equal_json);
}

TEST_P(PhraseFilterBenchTestCase, big_data_discrete_freqs) {
  estimateTimePhraseFilter(Config::big_data_freqs_discrete_json);
}


static constexpr auto kTestDirs = tests::GetDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(phrase_filter_bench_test, PhraseFilterBenchTestCase,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values(tests::FormatInfo{
                                              "1_5simd"})),
                         PhraseFilterBenchTestCase::to_string);
