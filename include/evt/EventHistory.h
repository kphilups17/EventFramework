#pragma once

#include "Event.h"

#include <list>
#include <map>
#include <memory>
#include <algorithm>
#include <iostream>
#include <type_traits>


namespace evt {


namespace detail {


struct None {};


} // namespace detail


// Type annotation for defining the clock type for an event.
// SFINAE friendly (the typename=void) to allow type classifiers to apply a clock.
template <typename _Event, typename=void> struct ClockFor { using value = detail::None; };


//Type annotation for defining if an event is posted late or early
struct OnTime {};
struct MaybeLate {};

template <typename _Event, typename=void> struct TimingFor { using value = OnTime; };


namespace detail {


// MaybeLate events can be posted OnTime or Late
// If they are posted late, we will nudge them ahead to make up the offset if clock-ticked
// OnTime events are always aligned, no messing around with them
template <typename _Event> using EventTime = typename TimingFor<_Event>::value;
template <typename _Event> bool constexpr maybe_late = std::is_same_v<EventTime<_Event>, MaybeLate>;


} // namespace detail


// Type alias for the type used to represent a stage.
using stage_t = int;


// A per-type stage value that is the stage number the event is posted in.
template <typename _Event> stage_t constexpr post_stage = 0;


// A wrapper type that associates an event instance with its current stage.
template <typename _Event>
struct StagedEvent {
  _Event event;
   stage_t stage;
};


// Simple types that represent various stages when interacting with the library.

// InRange - an inclusive stage range from begin to end.
struct InRange { stage_t begin; stage_t end; };

// InStage - a single stage.
struct InStage { stage_t stage; };

// UpTo - an inclusive stage range from the stage that the event is posted in, to the given stage.
struct UpTo { stage_t stage; };

// Last - a stage range of the most recent N stages of events.
struct Last { std::make_unsigned_t<stage_t> count; };

// Current - the single stage that the events are posted in.  Retrieves ONLY the events that have
// been posted, their clock has been seen, and PostDelayedEvents has happened.
struct Current {};

// PreClock - the single stage immediately before the one the events are posted in.  Retrieves
// ONLY the events that have been posted but have not yet seen a clock and PostDelayedEvents.
struct PreClock {};


namespace detail {


// Clock convenience types and constexprs.
template <typename _Event> using Clock = typename ClockFor<_Event>::value;
template <typename _Event> using ClockFilter = Filter<Clock<_Event>>;
template <typename _Event> bool constexpr has_clock = !std::is_same_v<Clock<_Event>, detail::None>;


// Type to wrap a stage value that makes it easier to interact with in std::tuple.
struct Stage { stage_t value; };

bool inline operator<(Stage lhs, Stage rhs) { return lhs.value < rhs.value; }


// Type to wrap a bool indicating that an event's clock has ticked this cycle.  Makes it easier to interact with in std::tuple.
struct ClockHasTicked { bool value; };


// History can be reconstructed from this data model, per filter function.
template <typename _Event>
using StagedHandlers = std::map<Stage, Handlers<_Event>>;

// The top-level of the data structure should be a linear collection of Filter/Data pairs
// so that registration order is what determines evaluation order, rather than the
// function address.  Registration order is more likely to be consistent from run to run,
// while the address of a function may be random in the presence of -fPIC and -fPIE
// flags and platform-specific security behaviors when dynamic linking is done.
template <typename _Event>
using FilteredHandlers = std::vector<std::tuple<Filter<_Event>, StagedHandlers<_Event>>>;


// A std::list of events because we will be inserting and erasing event instances
// very often, and don't need the ability to index into the staged events.
template <typename _Event>
using StagedEvents = std::list<StagedEvent<_Event>>;


using MaxStage = Stage;


template <typename _Event>
using EventHistory = std::tuple<detail::ClockFilter<_Event>, FilteredHandlers<_Event>, StagedEvents<_Event>, MaxStage, ClockHasTicked>;


} // namespace detail

// Range Comparator struct to be used as a predicate for std::equal_range
template <typename _Event>
struct InRangeCompare {
  bool operator()(InRange const &range, StagedEvent<_Event> const &event) const { return range.end < event.stage; }
  bool operator()(StagedEvent<_Event> const &event, InRange const &range) const { return event.stage < range.begin; }
};

// Configure the history to keep events for a given event filter and clock filter until at least the given stage.
template <typename _Event> void requireHistory(Filter<_Event> const &, detail::ClockFilter<_Event> const &, UpTo const &);

// Configure the history to keep events for a given event filter and clock filter for at least the given number of stages.
template <typename _Event> void requireHistory(Filter<_Event> const &, detail::ClockFilter<_Event> const &, Last const &);


// History is a proxy for accessing StagedEvents packaged with a filter function and a Range
// Is returned by the getHistory functions
// Can be used as the argument for the toVector and toTable function to convert into different containers
template <typename _Event>
struct History {
  detail::EventHistory<_Event> const *history = nullptr;
  Filter<_Event> filter = all<_Event>;
  InRange range = {post_stage<_Event>, post_stage<_Event>};

  // [] Operator for History returns a vector of event pointers that may exist in the stage
  // If no events exist in that stage, returns an empty vector.
  // Slightly less performant than toVector, about the same as toTable
  std::vector<const _Event*> operator[](int stage) const {
    if (stage < range.begin or range.end < stage) {
      throw std::runtime_error("History [] operator stage '" + std::to_string(stage) + "' is out of range [" + std::to_string(range.begin) + ", " + std::to_string(range.end) + "]");
    }
    std::vector<const _Event*> return_events{};
    // std:: equal_range does binary search of a sorted list of StagedEvents, returning a pair of iterators marking begin and end of the matching range.
    auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*history).begin(), std::get<detail::StagedEvents<_Event>>(*history).end(), InRange{stage, stage}, InRangeCompare<_Event>{});
    // We have to use a reverse iterator here because the list is stored youngest->oldest. 
    // When we fill up each stage we want to preserve the order that they were posted in that stage, so the oldest one should actually be at the front instead of the back. 
    // When we reverse the whole list we fill up the table in order of oldest->youngest
    // This doesn't matter in the toVector case because its only one event per stage
    for (auto iter = std::make_reverse_iterator(iter_range.second); iter != std::make_reverse_iterator(iter_range.first); ++iter) {
      if (filter->invoke(iter->event)) {
        return_events.push_back(&(iter->event));
      }
    }
    return return_events;
  }

  bool isValid(stage_t inStage) const {
    // if we have valid input args, check for a valid event in range, otherwise return false
    if (inStage < range.begin or range.end < inStage) {
      return false;
    }
    // std:: equal_range does binary search of a sorted list of StagedEvents, returning a pair of iterators marking begin and end of the matching range.
    auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*history).begin(), std::get<detail::StagedEvents<_Event>>(*history).end(), InRange{inStage, inStage}, InRangeCompare<_Event>{});
    for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
      if (filter->invoke(iter->event)) {
        return true;
      }
    }
    return false;
  }

  bool isValid(stage_t inStartStage, stage_t inEndStage) const {
    // if we have valid input args, check for a valid event in range, otherwise return false
    if (inStartStage < range.begin or range.end < inEndStage) {
      return false;
    }
    if(inStartStage > inEndStage) {
      return false;
    }
    // std:: equal_range does binary search of a sorted list of StagedEvents, returning a pair of iterators marking begin and end of the matching range.
    auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*history).begin(), std::get<detail::StagedEvents<_Event>>(*history).end(), InRange{inStartStage, inEndStage}, InRangeCompare<_Event>{});
    for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
      if (filter->invoke(iter->event)) {
        return true;
      }
    }
    return false;
  }

  template <stage_t ... Stages>
  bool isValid() const {
    return (isValid(Stages) || ...);
  }
};


// API functions to get a History object that provides structured access to the underlying data.
template <typename _Event> History<_Event> getHistory(Filter<_Event> const &, detail::ClockFilter<_Event> const &, InRange const &);
template <typename _Event, typename _Range> History<_Event> getHistory(Filter<_Event> const &, detail::ClockFilter<_Event> const &, _Range const &);
template <typename _Event, typename _Range> History<_Event> getHistory(detail::ClockFilter<_Event> const &, _Range const &);


//Definitions for MultiHandler forms
template <typename ..._Parameters>
using MultiHandler = std::function<void(_Parameters...)>;

template <typename ..._Parameters>
MultiHandler<_Parameters...> handler(void (*)(_Parameters...));

template <typename ..._Parameters, typename _Object>
MultiHandler<_Parameters...> handler(detail::MemberFunction<void(_Parameters...), _Object>, _Object*);


//Event Arg Type defintion
template <typename _Event, typename _Stage>
struct EventArg;


//EventArg creation function definitions
//Args that span more than one stage should have a InRange template arg
//Args that only have one stage can have a stage_t template arg
//Args that only have a filter and no delay can have void template params
template <typename _Event> EventArg<_Event, InRange> arg(Filter<_Event>, detail::ClockFilter<_Event>, InRange const &);
template <typename _Event> EventArg<_Event, InRange> arg(Filter<_Event>, detail::ClockFilter<_Event>, UpTo const &);
template <typename _Event> EventArg<_Event, InRange> arg(Filter<_Event>, detail::ClockFilter<_Event>, Last const &);
template <typename _Event> EventArg<_Event, stage_t> arg(Filter<_Event>, detail::ClockFilter<_Event>, InStage const &);
template <typename _Event> EventArg<_Event, stage_t> arg(Filter<_Event>, detail::ClockFilter<_Event>, Current const &);
template <typename _Event> EventArg<_Event, void> arg(Filter<_Event>);

template <typename _Event> EventArg<_Event, InRange> arg(detail::ClockFilter<_Event>, InRange const &);
template <typename _Event> EventArg<_Event, InRange> arg(detail::ClockFilter<_Event>, UpTo const &);
template <typename _Event> EventArg<_Event, InRange> arg(detail::ClockFilter<_Event>, Last const &);
template <typename _Event> EventArg<_Event, stage_t> arg(detail::ClockFilter<_Event>, InStage const &);
template <typename _Event> EventArg<_Event, stage_t> arg(detail::ClockFilter<_Event>, Current const &);
template <typename _Event> EventArg<_Event, void> arg();


// Delayed registerHandler overloads.

// Single event, const-reference.
template <typename _Event, typename _ClockFilter, typename _Stage>
void registerHandler(Handler<_Event> const &, EventArg<_Event, _Stage> const &);

// Multiple events, History<_Event>, vector<_Event const*> or const-pointer
template <typename ..._Events, typename ..._Args,  typename ..._Stages>
void registerHandler(MultiHandler<_Events...> const &, EventArg<_Args, _Stages> const &...);


//
//
// Implementation
//
//


namespace detail {


// Functions converting history stage descriptors to InRange instances.  Used to simplify the history backend by only needing to work with InRange.
// The history stage descriptors are used instead of functions returning an InRange instance to avoid the need for users to type <Event> in the API
// function call so the library can access post_stage<Event>.  Instead, these type-agnostic objects can be passed around and rendered internally
// once the event type is known to the library backend.
template <typename _Event> InRange toRange(InStage  const &stage    ) { return { stage.stage           , stage.stage                                  }; }
template <typename _Event> InRange toRange(UpTo     const &up_to    ) { return { post_stage<_Event>    , up_to.stage                                  }; }
template <typename _Event> InRange toRange(Last     const &last     ) { return { post_stage<_Event>    , stage_t(post_stage<_Event> + last.count - 1) }; }
template <typename _Event> InRange toRange(Current  const &current  ) { return { post_stage<_Event>    , post_stage<_Event>                           }; }
template <typename _Event> InRange toRange(PreClock const &pre_clock) { return { post_stage<_Event> - 1, post_stage<_Event> - 1                       }; }


template <typename _Event>
using EventHistories = std::vector<EventHistory<_Event>>;


//Functions the same way as HandlerManager but its data is the EventHistories vector
//Added to handler_manager list in Event.h and has it's clear function called
template <typename _Event>
struct EventHistoryManager : HandlerManagerBase {

  static EventHistoryManager<_Event> *getEventHistoryManager() {
    static EventHistoryManager<_Event> instance {};
    return &instance;
  }

  void clear() override {
    event_history.clear(); 
  }

  void clearHistory() override {
    for(auto& history : event_history){
      auto &staged_events = std::get<StagedEvents<_Event>>(history);
      staged_events.clear();
    } 
  }

  // [ ClockFilter, [ Filter, Stage -> [ Handler ] ], [ Event, Stage ], MaxStage, ClockHasTicked ]
  EventHistories<_Event> event_history;
};


struct MultiEventPosterBase {
  virtual ~MultiEventPosterBase() {}
  virtual void invokeHandler() = 0;
};


std::vector<std::unique_ptr<MultiEventPosterBase>> inline multi_event_posters;



struct ResetClockTicks {};
struct PostDelayedEvents {};
struct PostMultiEvents {};


// Abstract class to use as pointer in delayer_list
// This allows us to type-erase the Delayers for each event type
struct DelayerBase {
  virtual ~DelayerBase() = default;
  virtual void advanceAll() = 0;
  virtual void advancePreClock() = 0;
  virtual void handle() = 0;
  virtual bool anyUnhandleEvents() = 0;
  virtual void resetClockTick() = 0;
};

// This struct is used to advance the stage and call the handlers of each StagedEvent.
// Its functions are called in the PostDelayedEvents stage by processDelayedEvents()
// This struct is used to allow us to process each phase of processDelayedEvents for ALL event types before moving onto the next stage.
// The alternative strategy of using a templated processDelayedEvents function will call all phases for each event type one of time which does not functionally work for our approach.
template <typename _Event>
struct Delayer : DelayerBase {
  ~Delayer() override = default;

  // Advance all the events we have that have had their clocks ticked
  void advanceAll() override {
    // Phase 1: increment event stages and erase events that have gone past the end.
    for (auto &clock : detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history) {
      if (std::get<ClockHasTicked>(clock).value) {
        auto &staged_events = std::get<StagedEvents<_Event>>(clock);
        // If the event has reached the max stage, we can erase it.
        // All the oldest events should be at the back of the list and the rest of the list should be in order youngest->oldest (front->back)
        while (!staged_events.empty() && (staged_events.back().stage >= std::get<Stage>(clock).value)) {
          staged_events.pop_back();
        }
        for (auto &staged_event : staged_events) {
          ++staged_event.stage;
          mUnhandledStagedEventInfo.push_back({&std::get<FilteredHandlers<_Event>>(clock), &staged_event});
        }
      }
    }
  }

  // Call handlers for all the StagedEvents we have in our mUnHandledStagedEvents list
  void handle() override {
    for (auto &event_info : mUnhandledStagedEventInfo) {
      auto staged_event = std::get<StagedEvent<_Event> const *>(event_info);
      auto &event = staged_event->event;
      auto &stage = staged_event->stage;
      for (auto &filtered_handlers : *std::get<FilteredHandlers<_Event> const *>(event_info)) {
        if (std::get<Filter<_Event>>(filtered_handlers)->invoke(event)) {
          auto &staged_handlers = std::get<StagedHandlers<_Event>>(filtered_handlers);
          auto iter = staged_handlers.find({stage});
          if (iter != staged_handlers.end()) {
            for (auto &handler : iter->second) {
              handler(event);
            }
          }
        }
      }
    }
    mUnhandledStagedEventInfo.clear();
  }

  // After handle is called, we may have new events that have been posted
  // Check if we have any events in pre-clock now that have had a clock tick and increment them/call their handlers
  // This happens in postDelayedEvents
  void advancePreClock() override {
    for (auto &clock : detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history) {
      if (std::get<ClockHasTicked>(clock).value) {
        auto &staged_events = std::get<StagedEvents<_Event>>(clock);
        for (auto &staged_event : staged_events) {
          // check if we have any new pre-clock postings and advance
          if (staged_event.stage == (post_stage<_Event> - 1)) {
            ++staged_event.stage;
            mUnhandledStagedEventInfo.push_back({&std::get<FilteredHandlers<_Event>>(clock), &staged_event});
          }
        }
      }
    }
  }

  bool anyUnhandleEvents() override {
    return mUnhandledStagedEventInfo.size();
  }

  void resetClockTick() override {
    for (auto &clock : detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history) {
      std::get<ClockHasTicked>(clock).value = false;
    }
  }

  // List of all events, filters, and handlers that we need to process each loop through processDelayedEvents
  std::list<std::tuple<FilteredHandlers<_Event> const *, StagedEvent<_Event> const *>> mUnhandledStagedEventInfo;
};


// Type-erased list of all Delayers used in the EventHistory
std::vector<std::unique_ptr<DelayerBase>> inline delayer_list;


// Reset clock ticks before each PostDelayedEvents callback - called in EventReset by FusionClockHandler.h
inline void resetClockTicks(ResetClockTicks const &) {
  for (auto &delayer : delayer_list) {
    delayer->resetClockTick();
  }
}


// Delayed events are managed in distinct phases. For each phase, we process all events in our history of all types before moving onto the next stage.
// Doing this is a functional requirement for correctly handling event history. 
// The phases are as follows
//   1. We should have reset all ClockHasTicked vars before this callback - (i.e. in EventReset callback)
//   2. The current stage of events is incremented if their clock has ticked.  Events that exceed their max stage are erased. We add all these events to a list of "Unhandled Events".
//   3. Handlers are called with the events in the "Unhandled Events" list. Each event is removed from the list of "Unhandled Events".
//   4. We check to see if there are any new events posted in the handler run in phase#2 by checking for all PreClock events. We increment the stage of these events and add them to a list of "Unhandled Events".
//   5. We repeat phase#3->phase#5 until the "Unhandled Events" list is empty for all events of all types.
// The getHistory functions expose history information about event occurrences, aligned to their clocks.  A call to a
// getHistory function will not include any events that have been posted since the last posting of PostDelayedEvents.
// That means that calls to getHistory won't show events even even after their clock is posted, until the next time
// PostDelayedEvents is posted as well.
inline void postDelayedEvents(PostDelayedEvents const &) {
  // Iterating over all event types and advance their stage
  for (auto &delayer : delayer_list) {
    delayer->advanceAll();
  }
  // Iterating over all event types and call handlers
  for (auto &delayer : delayer_list) {
    delayer->handle();
  }
  // Need to see if handlers posted new events that need to be staged
  bool handledSomeEvents = true;
  while (handledSomeEvents) {
    handledSomeEvents = false;
    // Check all pre-clocked events and increment their stage for all types
    for (auto &delayer : delayer_list) {
      delayer->advancePreClock();
    }
    // Check if there are new events posted that need to be handled
    for (auto &delayer : delayer_list) {
      if (delayer->anyUnhandleEvents()) {
        handledSomeEvents = true;
        delayer->handle();
      }
    }
    // After this stage more events may have been posted in the handler that need to be handled again
    // By looping through again we will repeat the process until there are no more events posted
  }
}

inline void postMultiEvents(PostMultiEvents const &) {
  // Invoke MultiEventPosters after everything is done and settled
  // Any posts that happen during this callback will count as happening in the next cycle
  // Has to be here before reset clock ticks so we know the clock has ticked
  // This call should happen in the `isPostingLate()==true` stage
  for (auto &mep : detail::multi_event_posters) {
    mep->invokeHandler();
  }
}


template <typename _Event>
void processClock(Clock<_Event> const &clock) {
  for (auto &item : detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history) {
    if (std::get<detail::ClockFilter<_Event>>(item)->invoke(clock)) {
      std::get<ClockHasTicked>(item).value = true;
    }
  }
}


// Allows a determination of whether we are after the point that post_delayed events has run in a sim cycle
// Behavior can be defined by external source 
inline bool alwaysFalse() { return false; }
inline std::function<bool()> isPostingLate = alwaysFalse;


template <typename _Event>
void processEvent(_Event const &event) {
  for (auto &clock : detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history) {
    // For now we need to save all events, we can only check the filter once we call handlers or when the history is polled.
    // This is because we can't be sure Filters won't produce different results at different times.
    // If it is a late event we can offset by 1.

    // MaybeLate events can be posted OnTime or Late
    // If they are posted late, we will nudge them ahead to make up the offset if clock-ticked
    // OnTime events are always aligned, no messing around with them
    if constexpr (detail::maybe_late<_Event>) {
      if (std::get<ClockHasTicked>(clock).value && isPostingLate()) {
        std::get<StagedEvents<_Event>>(clock).push_front({event, post_stage<_Event>});
      } else {
        std::get<StagedEvents<_Event>>(clock).push_front({event, post_stage<_Event> - 1});
      }
    } else {
      std::get<StagedEvents<_Event>>(clock).push_front({event, post_stage<_Event> - 1});
    }
    break;
  }
}


template <typename _Event>
EventHistory<_Event> &getEventHistory(ClockFilter<_Event> const &in_clock_filter) {
  for (auto &clock : detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history) {
    if (std::get<ClockFilter<_Event>>(clock) == in_clock_filter) {
      return clock;
    }
  }
  // If we made it here, no matching clock filter was found, so we can add a new one.
  detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history.push_back({in_clock_filter, {}, {}, {post_stage<_Event>}, {false}});
  return detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history.back();
}

// This requireHistory struct is in detail namespace
// It is used internally by the library to setup our MaxStage and register library handlers for the event and clock
// In addition it sets up the delayer object and registers the PostDelayedEvents callback
// Lastly it returns a reference to the EventHistory<_Event> tuple it is registering
// This tuple is passed forward to the getHistory functions to construct a History proxy for users to interact with
template <typename _Event>
EventHistory<_Event> &requireHistory(detail::ClockFilter<_Event> const &in_clock_filter, UpTo const &range) {
  // if nothing has registered at all yet, created the callback for postDelayedEvents
  if (detail::delayer_list.empty()) {
    registerHandler<detail::PostDelayedEvents>(detail::postDelayedEvents);
    registerHandler<detail::PostMultiEvents>(detail::postMultiEvents);
    registerHandler<detail::ResetClockTicks>(detail::resetClockTicks);
  }
  if (detail::EventHistoryManager<_Event>::getEventHistoryManager()->event_history.empty()) {
    // if it is the first history for event of this type, make a new delayer and set up the processEvent/Clock callbacks
    detail::delayer_list.push_back(std::make_unique<detail::Delayer<_Event>>());
    registerHandler(handler(detail::processEvent<_Event>));
    registerHandler(handler(detail::processClock<_Event>));
  }
  auto &history = detail::getEventHistory<_Event>(in_clock_filter);
  auto &stage = std::get<detail::MaxStage>(history).value;
  stage = std::max(stage, range.stage);
  return history;
}


// Clears lists of handlers, event_histories, multi-poster list and delayer_list to reset everything to a clean state
inline void clearEventData() {
  clearHandlers();
  multi_event_posters.clear();
  delayer_list.clear();
}


// Only clears the list of StagedEvents in the EventHistoryManager
inline void clearHistory() {
  clearHandlerHistory();
}


} // namespace detail


// Public facing requireHistory api
template <typename _Event>
void requireHistory(detail::ClockFilter<_Event> const &in_clock_filter, stage_t in_stage) {
  detail::requireHistory<_Event>(in_clock_filter, UpTo{in_stage});
}

template <typename _Event>
void requireHistory(detail::ClockFilter<_Event> const &in_clock_filter, UpTo const &range) {
  detail::requireHistory<_Event>(in_clock_filter, range);
}

template <typename _Event>
void requireHistory(detail::ClockFilter<_Event> const &in_clock_filter, Last const &range) {
  detail::requireHistory<_Event>(in_clock_filter, UpTo{stage_t(post_stage<_Event> + range.count - 1)});
}


// Base registerHandler call with a delay
template <typename _Event>
void registerHandler(Handler<_Event> const &in_handler, EventArg<_Event, stage_t> const &in_arg) {
  // If we see that you register for a callback before or during the post_stage, we can throw and error cause you will never be called
  if constexpr (detail::maybe_late<_Event>) {
    if (in_arg.stage <= post_stage<_Event>) {
      // Get and demangle typename for error message
      std::string tname = evt::detail::getEventName<_Event>();
      throw std::runtime_error("callback for MaybeLate event is not possible, callback must be later than post_stage<" + tname + "> = " + std::to_string(post_stage<_Event>));
    }
  } else {
    if (in_arg.stage <= (post_stage<_Event> - 1)) {
      // Get and demangle typename for error message
      std::string tname = evt::detail::getEventName<_Event>();
      throw std::runtime_error("callback for OnTime event is not possible, callback must be later than (post_stage<" + tname + "> - 1) = " + std::to_string(post_stage<_Event>-1));
    }
  }
  auto &clock = detail::requireHistory<_Event>(in_arg.clock_filter, UpTo{in_arg.stage});
  auto &filtered_handlers = std::get<detail::FilteredHandlers<_Event>>(clock);
  for (auto &filter_and_handlers : filtered_handlers) {
    // When we find a matching filter function, append the handler and return.
    if (std::get<Filter<_Event>>(filter_and_handlers) == in_arg.filter) {
      std::get<detail::StagedHandlers<_Event>>(filter_and_handlers)[{in_arg.stage}].push_back(in_handler);
      return;
    }
  }
  // If we got here didn't find any matching filters in the FilteredHandlers list
  // Add new filter to back of FilteredHandlers and then add handler in the correct stage
  filtered_handlers.push_back({in_arg.filter, {}});
  std::get<detail::StagedHandlers<_Event>>(filtered_handlers.back())[{in_arg.stage}].push_back(in_handler);
}


namespace detail {


template <typename _Event, typename _Payload>
struct HistorySeries {
  InRange range;
  std::vector<_Payload> data;

  HistorySeries(InRange const &in_range): range(in_range), data(range.end - range.begin + 1, _Payload{}) {}

  using const_iterator = typename decltype(data)::const_iterator;

  _Payload const &front() const { return data.front(); }
  _Payload const &back() const { return data.back(); }

  const_iterator begin() const { return data.begin(); }
  const_iterator end() const { return data.end(); }

  // Index operator should be able to index directly by stage, use mRange.begin to offset into appropriate vector location
  _Payload const &operator[](int stage) {
    if (stage < range.begin or range.end < stage) {
      throw std::runtime_error("History [] operator stage '" + std::to_string(stage) + "' is out of range [" + std::to_string(range.begin) + ", " + std::to_string(range.end) + "]");
    }
    return data[stage - range.begin];
  }
};


template <typename _T>
constexpr bool is_valid_multi_handler_parameter = false;

template <typename _Event>
constexpr bool is_valid_multi_handler_parameter<_Event const *>  = true;

template <typename _Event>
constexpr bool is_valid_multi_handler_parameter<std::vector<_Event const *> const &>  = true;

template <typename _Event>
constexpr bool is_valid_multi_handler_parameter<History<_Event> const &>  = true;


} // namespace detail


// A densely populated vector of event pointers in a stage range.
// It is indexed by stage, and will provide access to a single event per stage.
// If you need multiple events to be able to occupy the same stage at once, use HistoryTable.
template <typename _Event>
using HistoryVector = detail::HistorySeries<_Event, _Event const *>;

// A densely populated vector of event pointer vectors in a stage range.
// It is indexed by stage, and will provide access to all events in that stage.
// If you only have one event per stage, use HistoryVector to get direct access to that event pointer.
template <typename _Event>
using HistoryTable = detail::HistorySeries<_Event, std::vector<_Event const *>>;


//Functions to check the state of History, HistoryVector and HistoryTable objects

//Function to check if there are no events in the History object
//Will return true if no events are within in the range of the History
template <typename _Event>
bool isEmpty(History<_Event> const &in_history) {
  auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*in_history.history).begin(), std::get<detail::StagedEvents<_Event>>(*in_history.history).end(), in_history.range, InRangeCompare<_Event>{});
  for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
    if (in_history.filter->invoke(iter->event)) {
      return false;
    }
  }
  return true;
}

//Function to check if there are no events in the HistoryVector object
//Will return true if are only nullptrs in the vector
template <typename _Event>
bool isEmpty(HistoryVector<_Event> const &in_vector) {
  for (auto stage : in_vector) {
    if (stage!=nullptr) {
      return false;
    }
  }
  return true;
}

//Function to check if there are no events in the HistoryTable object
//Will return true if there are only empty vectors in the table
template <typename _Event>
bool isEmpty(HistoryTable<_Event> const &in_table) {
  for (auto& stage : in_table) {
    if (stage.size()) {
      return false;
    }
  }
  return true;
}

//Function to check the number of populated element of a History object
//Will return the total number of events in the range
template <typename _Event>
size_t popcount(History<_Event> const &in_history) {
  auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*in_history.history).begin(), std::get<detail::StagedEvents<_Event>>(*in_history.history).end(), in_history.range, InRangeCompare<_Event>{});
  size_t count = 0;
  for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
    if (in_history.filter->invoke(iter->event)) {
      ++count;
    }
  }
  return count;
}

//Function to check the number of populated element of a HistoryVector object
//Will return the number of non-nullptrs in the vector
template <typename _Event>
size_t popcount(HistoryVector<_Event> const &in_vector) {
  size_t size = 0;
  for (auto stage : in_vector) {
    if (stage != nullptr) {
      ++size;
    }
  }
  return size;
}

//Function to check the number of populated element of a HistoryTable object
//Will return the total number of events in the table
//TODO - do we need separate function to check number of populated vectors?
template <typename _Event>
size_t popcount(HistoryTable<_Event> const &in_table) {
  size_t size = 0; 
  for (auto& stage : in_table) {
    for (const auto& event : stage) {
      if (event != nullptr) {
         ++size;
      }
    }
  }
  return size;
}

// Functions to convert a History to a HistoryVector or HistoryTable.

template <typename _Event>
HistoryVector<_Event> toVector(History<_Event> const &in_history) {
  // std:: equal_range does binary search of a sorted list of StagedEvents, returning a pair of iterators marking begin and end of the matching range.
  auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*in_history.history).begin(), std::get<detail::StagedEvents<_Event>>(*in_history.history).end(), in_history.range, InRangeCompare<_Event>{});
  HistoryVector<_Event> history_vector {in_history.range};
  for (auto iter = iter_range.first; iter != iter_range.second; ++iter) {
    if (in_history.filter->invoke(iter->event)) {
      history_vector.data[iter->stage - in_history.range.begin] = &(iter->event);
    }
  }
  return history_vector;
}

template <typename _Event>
HistoryTable<_Event> toTable(History<_Event> const &in_history) {
  // std:: equal_range does binary search of a sorted list of StagedEvents, returning a pair of iterators marking begin and end of the matching range.
  auto iter_range = std::equal_range(std::get<detail::StagedEvents<_Event>>(*in_history.history).begin(), std::get<detail::StagedEvents<_Event>>(*in_history.history).end(), in_history.range, InRangeCompare<_Event>{});
  HistoryTable<_Event> history_table {in_history.range};
  
  // We have to use a reverse iterator here because the list is stored youngest->oldest. 
  // When we fill up each stage we want to preserve the order that they were posted in that stage, so the oldest one should actually be at the front instead of the back. 
  // When we reverse the whole list we fill up the table in order of oldest->youngest
  // This doesn't matter in the toVector case because its only one event per stage
  for (auto iter = std::make_reverse_iterator(iter_range.second); iter != std::make_reverse_iterator(iter_range.first); ++iter) {
    if (in_history.filter->invoke(iter->event)) {
      history_table.data[iter->stage - in_history.range.begin].push_back(&(iter->event));
    }
  }
  return history_table;
}

// Check Range Bounds
template <typename _Event>
void checkRange(InRange const &range) {
  if (range.end < range.begin) {
    // Get and demangle typename for error message
    std::string tname = evt::detail::getEventName<_Event>();
    throw std::runtime_error("Requested History Range end is less than start for event " + tname + " end = " + std::to_string(range.end) + ", begin = " + std::to_string(range.begin));
  }
  if constexpr (detail::maybe_late<_Event>) {
    if (range.begin < post_stage<_Event>) {
      // Get and demangle typename for error message
      std::string tname = evt::detail::getEventName<_Event>();
      throw std::runtime_error("Requested History Range for MaybeLate event " + tname + " begin = " + std::to_string(range.begin) + ", less than than post_stage<" + tname + "> = " + std::to_string(post_stage<_Event>));
    }
  } else {
    if (range.begin < (post_stage<_Event> - 1)) {
      // Get and demangle typename for error message
      std::string tname = evt::detail::getEventName<_Event>();
      throw std::runtime_error("Requested History Range for OnTime event " + tname + " begin = " + std::to_string(range.begin) + ", less than than (post_stage<" + tname + "> - 1) = " + std::to_string(post_stage<_Event>-1));
    }
  }
}

// Begin getHistory API
template <typename _Event>
History<_Event> getHistory(Filter<_Event> const &in_filter, detail::ClockFilter<_Event> const &in_clock_filter, InRange const &range) {
  auto &history = detail::requireHistory<_Event>(in_clock_filter, UpTo{range.end});
  checkRange<_Event>(range);
  if (range.end > std::get<detail::MaxStage>(history).value) {
    // Get and demangle typename for error message
    std::string tname = evt::detail::getEventName<_Event>();
    throw std::runtime_error("Requested History Range for event " + tname + " end = " + std::to_string(range.end) + " is greater than max allocated history depth");
  }
  return {&history, in_filter, range};
}

template <typename _Event, typename _Range>
History<_Event> getHistory(Filter<_Event> const &in_filter, detail::ClockFilter<_Event> const &in_clock_filter, _Range const &range) {
  return getHistory<_Event>(in_filter, in_clock_filter, detail::toRange<_Event>(range));
}

template <typename _Event, typename _Range>
History<_Event> getHistory(detail::ClockFilter<_Event> const &in_clock_filter, _Range const &range) {
  return getHistory<_Event>(all<_Event>, in_clock_filter, detail::toRange<_Event>(range));
}

//Function to create multi handler from a free function
template <typename ..._Parameters>
MultiHandler<_Parameters...> handler(void (*in_handler)(_Parameters...)) {
  static_assert((detail::is_valid_multi_handler_parameter<_Parameters> && ...), "MultiHandler parameters must be 'Event const *' or 'History<Event> const &'");
  return {in_handler};
}

//Function to create multi handler from a member function
template <typename ..._Parameters, typename _Object>
MultiHandler<_Parameters...> handler(detail::MemberFunction<void(_Parameters...), _Object> in_function, _Object *in_object){
  static_assert((detail::is_valid_multi_handler_parameter<_Parameters> && ...), "MultiHandler parameters must be 'Event const *' or 'History<Event> const &'");
  return { [in_object, in_function](_Parameters ... params) { return (in_object->*in_function)(params ...); } };
}

//Three types of EventArg structs
//1. For Args that span a range of stages
//2. For Args that only have a single stage
//3. For Args that are for non-delayed callbacks
template <typename _Event>
struct EventArg<_Event, InRange> {
  Filter<_Event> filter;
  detail::ClockFilter<_Event> clock_filter;
  InRange range;
};

template <typename _Event>
struct EventArg<_Event, stage_t> {
  Filter<_Event> filter;
  detail::ClockFilter<_Event> clock_filter;
  stage_t stage;
};

template <typename _Event>
struct EventArg<_Event, void> {
  Filter<_Event> filter;
};


//
// arg implementations
//


template <typename _Event>
EventArg<_Event, InRange> arg(Filter<_Event> in_filter, detail::ClockFilter<_Event> in_clock_filter, InRange const &in_range) {
  return { in_filter, in_clock_filter, in_range };
}

template <typename _Event>
EventArg<_Event, InRange> arg(Filter<_Event> in_filter, detail::ClockFilter<_Event> in_clock_filter, UpTo const &in_up_to) {
  return arg(in_filter, in_clock_filter, detail::toRange<_Event>(in_up_to));
}

template <typename _Event>
EventArg<_Event, InRange> arg(Filter<_Event> in_filter, detail::ClockFilter<_Event> in_clock_filter, Last const &in_last) {
  return arg(in_filter, in_clock_filter, detail::toRange<_Event>(in_last));
}

template <typename _Event>
EventArg<_Event, stage_t> arg(Filter<_Event> in_filter, detail::ClockFilter<_Event> in_clock_filter, InStage const &in_stage) {
  return { in_filter, in_clock_filter, in_stage.stage };
}

template <typename _Event>
EventArg<_Event, stage_t> arg(Filter<_Event> in_filter, detail::ClockFilter<_Event> in_clock_filter, Current const &in_current) {
  return arg(in_filter, in_clock_filter, InStage{post_stage<_Event>});
}

template <typename _Event>
EventArg<_Event, void> arg(Filter<_Event> in_filter) {
  return { in_filter };
}


template <typename _Event>
EventArg<_Event, InRange> arg(detail::ClockFilter<_Event> in_clock_filter, InRange const &in_range) {
  return arg(all<_Event>, in_clock_filter, in_range);
}

template <typename _Event>
EventArg<_Event, InRange> arg(detail::ClockFilter<_Event> in_clock_filter, UpTo const &in_up_to) {
  return arg(all<_Event>, in_clock_filter, in_up_to);
}

template <typename _Event>
EventArg<_Event, InRange> arg(detail::ClockFilter<_Event> in_clock_filter, Last const &in_last) {
  return arg(all<_Event>, in_clock_filter, in_last);
}

template <typename _Event>
EventArg<_Event, stage_t> arg(detail::ClockFilter<_Event> in_clock_filter, InStage const &in_stage) {
  return arg(all<_Event>, in_clock_filter, in_stage);
}

template <typename _Event>
EventArg<_Event, stage_t> arg(detail::ClockFilter<_Event> in_clock_filter, Current const &in_current) {
  return arg(all<_Event>, in_clock_filter, in_current);
}

template <typename _Event>
EventArg<_Event, void> arg() {
  return arg(all<_Event>);
}


//MultiEvent detail support
namespace detail {

//getHistory functions that explicitly accept EventArg
//used in the MultiEvent registerHandler function
template <typename _Event>
History<_Event> getHistory(EventArg<_Event, void> const &arg) {
  return getHistory(arg.filter, arg.clock_filter, InRange{post_stage<_Event>, post_stage<_Event>});
}

template <typename _Event>
History<_Event> getHistory(EventArg<_Event, stage_t> const &arg) {
  return getHistory(arg.filter, arg.clock_filter, InRange{arg.stage, arg.stage});
}

template <typename _Event>
History<_Event> getHistory(EventArg<_Event, InRange> const &arg) {
  return getHistory(arg.filter, arg.clock_filter, arg.range);
}


//
// Map an event pointer or event history to it's history type.
//   For event pointers, results in History<Event>.
//   For const-references to a history, results in the history type without the reference.
//

template <typename _T> struct multi_handler_history_f;
template <typename _T> using MultiHandlerHistory = typename multi_handler_history_f<_T>::value;

template <typename _Event>
struct multi_handler_history_f<_Event const *> {
  using value = History<_Event>;
};

template <typename _Event>
struct multi_handler_history_f<std::vector<_Event const *> const&> {
  using value = History<_Event>;
};

template <typename _Event>
struct multi_handler_history_f<History<_Event> const &> {
  using value = History<_Event>;
};

//Functions to pass through the right data to our multi-handler functions
//Detects what type of data is expected by the handler and correctly extracts that data from the History argument

//If the parameter is a History<_Event>, we can simply pass through the History struct - this would be for range based callbacks
template <typename _Parameter, typename _Event>
std::enable_if_t<(std::is_same_v<_Parameter, History<_Event> const &>), History<_Event> const &>
toParameter(History<_Event> const &history) {
  return history;
}

//Single Stage Callback
//If the paramter is a vector<_Event const*>, pass through a vector of all the events in that stage by converting to a HistoryTable and passing .front() (i.e. the only element)
template <typename _Parameter, typename _Event>
std::enable_if_t<(std::is_same_v<_Parameter, std::vector<_Event const *> const &>), std::vector<_Event const *>>
toParameter(History<_Event> const &history) {
  return toTable(history).front();
}

//Single Stage Callback
//If the paramter is a _Event const*, just pass through one of the events by converting to a vector and taking the first (and only) element
template <typename _Parameter, typename _Event>
std::enable_if_t<(std::is_same_v<_Parameter, _Event const *>), _Event const *>
toParameter(History<_Event> const &history) {
  return toVector(history).front();
}


template <typename ..._Parameters>
struct MultiEventPoster : MultiEventPosterBase {
  MultiHandler<_Parameters...> handler;
  std::tuple<MultiHandlerHistory<_Parameters>...> histories;

  MultiEventPoster(
    MultiHandler<_Parameters...> const &in_handler,
    MultiHandlerHistory<_Parameters> const &...in_args
  ):
    handler(in_handler),
    histories{ in_args... }
  {}

  virtual ~MultiEventPoster() {}

  void invokeHandler() override {
    // At least one non-empty history's clock ticked.
    if (std::apply([](auto const &...params) { return ((std::get<detail::ClockHasTicked>(*params.history).value && !isEmpty(params)) || ...); }, histories)) {
      std::apply([this](auto const &...params) { handler(toParameter<_Parameters>(params)...); }, histories);
    }
  }
};


// Used to check if our paramaters are compatible with the Range/Stage provided in the EventArg
// i.e. History is only compatible with a multi-stage Range
// Vector is only compatible with a single stage
// Pointer is only compatible with a single stage
template <typename _Parameter, typename _Event, typename _Stage>
inline constexpr bool is_compatible = false;

template <typename _Event> inline constexpr bool is_compatible<History<_Event> const &, _Event, InRange> = true;
template <typename _Event> inline constexpr bool is_compatible<std::vector<_Event const *> const &, _Event, stage_t> = true;
template <typename _Event> inline constexpr bool is_compatible<_Event const *, _Event, stage_t> = true;
template <typename _Event> inline constexpr bool is_compatible<_Event const &, _Event, stage_t> = true;
template <typename _Event> inline constexpr bool is_compatible<_Event const *, _Event, void> = false;
template <typename _Event> inline constexpr bool is_compatible<_Event const &, _Event, void> = false;


} // namespace detail


// registerHandler overload for multi-event handlers.
// This overload requires an event arg, which contains basically the same information as a History, but is specialized
// with the parameter type, not the event itself.  Allowed parameter types are `Event const *` and `History<Event> const &`.
// The event arg is used to check at compile time that History parameters register for histories of the right depth, and
// that Event pointer parameters are associated with exactly one stage.
template <typename ..._Events, typename ..._Args, typename ..._Stages>
void registerHandler(
  MultiHandler<_Events...> const &handler,
  EventArg<_Args, _Stages> const &...args
) {
  static_assert(!(sizeof...(_Args) < sizeof...(_Events)), "Not enough args provided to multi-event registerHandler call");
  static_assert(!(sizeof...(_Args) > sizeof...(_Events)), "Too many args provided to multi-event registerHandler call");
  // For only one event, register a regular delayed callback.
  // If the arg has a _Stage of void, that means it should be an immediate callback for a single event; this should register a
  //   callback with the base functions from Event.h, and has no ClockFilter.
  // If the arg has a _Stage of InStage, it should register a normal delayed callback for a single event.
  // If the arg has a _Stage of InRange, it should register for a delayed callback with a History.
  using stage_type = std::tuple_element_t<0, std::tuple<_Stages...>>;
  if constexpr (sizeof...(_Events) == 1 && std::is_same_v<stage_type, void>) {
    registerHandler(handler, args.filter...);
  } else {
    static_assert(
      (detail::is_compatible<_Events, _Args, _Stages> && ...),
      "Mismatching arg for event type.  History parameters must match a arg call with InRange, event pointer parameters must match a arg call with a single stage number."
    );
    detail::multi_event_posters.push_back(std::make_unique<detail::MultiEventPoster<_Events...>>(handler, detail::getHistory(args)...));   
  }
}


} // namespace evt
