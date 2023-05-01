#pragma once

#include <iostream>
#include <variant>
#include <vector>

#include "envoy/server/admin.h"

#include "source/server/admin/stats_params.h"
#include "source/server/admin/stats_render.h"
#include "source/server/admin/utils.h"

#include "absl/container/btree_map.h"
#include "absl/types/variant.h"

namespace Envoy {
namespace Server {

// Captures context for a streaming request, implementing the AdminHandler interface. The class
// templating allows derived classes to specify how each stat type (Text Readout, Counter, etc.)
// is represented, catering for different exposition formats (e.g. grouped, ungrouped).
template <class TextReadoutType, class CounterType, class GaugeType, class HistogramType>
class StatsRequest : public Admin::Request {

public:
  static constexpr uint64_t DefaultChunkSize = 2 * 1000 * 1000;

  using UrlHandlerFn = std::function<Admin::UrlHandler()>;

  // Admin::Request
  Http::Code start(Http::ResponseHeaderMap& response_headers) override;

  // Streams out the next chunk of stats to the client, visiting only the scopes
  // that can plausibly contribute the next set of named stats. This enables us
  // to linearly traverse the entire set of stats without buffering all of them
  // and sorting.
  //
  // Instead we keep the a set of candidate stats to emit in stat_map_ an
  // alphabetically ordered btree, which heterogeneously stores stats of all
  // types and scopes. Note that there can be multiple scopes with the same
  // name, so we keep same-named scopes in a vector. However leaf metrics cannot
  // have duplicates. It would also be feasible to use a multi-map for this.
  //
  // So in start() above, we initially populate all the scopes, as well as the
  // metrics contained in all scopes with an empty name. So in nextChunk we can
  // emit and remove the first element of stat_map_. When we encounter a vector
  // of scopes then we add the contained metrics to the map and continue
  // iterating.
  //
  // Whenever the desired chunk size is reached we end the current chunk so that
  // the current buffer can be flushed to the network. In #19898 we will
  // introduce flow-control so that we don't buffer the all the serialized stats
  // while waiting for a slow client.
  //
  // For text/JSON/HTML stats, we do 3 passes through all the scopes_, and we emit
  // text-readouts first, then the intermingled counters and gauges, and finally
  // the histograms. For prometheus stats, we do 3 or 4 passes (this depends on whether Text
  // Readouts are explicitly requested) through all the scopes_, and we emit counters first,
  // then gauges, possibly text readouts and finally histograms.
  bool nextChunk(Buffer::Instance& response) override;

  // To duplicate prior behavior for this class, we do several passes over all the stats. For
  // text/JSON/HTML stats, we do 3 passes:
  //   1. text readouts across all scopes
  //   2. counters and gauges, co-mingled, across all scopes
  //   3. histograms across all scopes.
  // For prometheus stats, we do 3 or 4 passes:
  //   1. counters across all scopes
  //   2. gauges across all scopes
  //   3. text readouts (only if explicitly requested via query param)
  //   4. histograms across all scopes.
  // It would be little more efficient to co-mingle all the stats, but three
  // passes over the scopes is OK. In the future we may decide to organize the
  // result data differently, but in the process of changing from buffering
  // the entire /stats response to streaming the data out in chunks, it's easier
  // to reason about if the tests don't change their expectations.
  void startPhase();

  // Sets the chunk size.
  void setChunkSize(uint64_t chunk_size) { chunk_size_ = chunk_size; }

protected:
  // Ordered to match the StatsOrScopes variant.
  enum class StatOrScopesIndex { Scopes, TextReadout, Counter, Gauge, Histogram };

  // In order to keep the output consistent with the fully buffered behavior
  // prior to the chunked implementation that buffered each type, we iterate
  // over all scopes for each type. This enables the complex chunking
  // implementation to pass the tests that capture the buffered behavior. There
  // is not a significant cost to this, but in a future PR we may choose to
  // co-mingle the types. Note that histograms are grouped together in the data
  // JSON data model, so we won't be able to fully co-mingle.
  enum class Phase {
    TextReadouts,
    CountersAndGauges,
    Counters,
    Gauges,
    Histograms,
  };

  using ScopeVec = std::vector<Stats::ConstScopeSharedPtr>;

  using StatOrScopes =
      absl::variant<ScopeVec, TextReadoutType, CounterType, GaugeType, HistogramType>;

  StatsRequest(Stats::Store& stats, const StatsParams& params,
               UrlHandlerFn url_handler_fn = nullptr);

  ~StatsRequest() override = default;

  // Iterates over scope_vec and populates the metric types associated with the
  // current phase.
  void populateStatsForCurrentPhase(const ScopeVec& scope_vec);

  virtual Stats::IterateFn<Stats::TextReadout> saveMatchingStatForTextReadout() PURE;
  virtual Stats::IterateFn<Stats::Gauge> saveMatchingStatForGauge() PURE;
  virtual Stats::IterateFn<Stats::Counter> saveMatchingStatForCounter() PURE;
  virtual Stats::IterateFn<Stats::Histogram> saveMatchingStatForHistogram() PURE;

  virtual void processTextReadout(const std::string& name, Buffer::Instance& response,
                                  const StatOrScopes& variant) PURE;
  virtual void processGauge(const std::string& name, Buffer::Instance& response,
                            const StatOrScopes& variant) PURE;
  virtual void processCounter(const std::string& name, Buffer::Instance& response,
                              const StatOrScopes& variant) PURE;
  virtual void processHistogram(const std::string& name, Buffer::Instance& response,
                                const StatOrScopes& variant) PURE;

  virtual void setRenderPtr(Http::ResponseHeaderMap& response_headers) PURE;
  virtual StatsRenderBase& render() PURE;

  StatsParams params_;
  absl::btree_map<std::string, StatOrScopes> stat_map_;
  Buffer::OwnedImpl response_;
  UrlHandlerFn url_handler_fn_;

  // Phase-related state.
  uint64_t phase_stat_count_{0};
  uint32_t phase_index_{0};
  std::vector<Phase> phases_;

private:
  Stats::Store& stats_;
  ScopeVec scopes_;
  uint64_t chunk_size_{DefaultChunkSize};
  std::map<Phase, std::string> phase_labels_{{Phase::TextReadouts, "Text Readouts"},
                                             {Phase::CountersAndGauges, "Counters and Gauges"},
                                             {Phase::Histograms, "Histograms"},
                                             {Phase::Counters, "Counters"},
                                             {Phase::Gauges, "Gauges"}};
};

} // namespace Server
} // namespace Envoy
