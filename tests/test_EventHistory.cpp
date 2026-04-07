// TODO: Can we provide a mechanism for handling cases where the same event is
//       is posted in the same clock tick with the same ID multiple times?  We
//       currently just collect all of them, and will post each one when the
//       PostDelayedEvents event is posted.  This should be a compile-time switch
//       per event type, so not all events have to behave the same way.  Should
//       this include the ability to categorically disable history/delay?
//       - Allow multiple (status quo)
//       - Invoke user-registered "collision" handler, keeping the result:
//           _Event resolve(_Event const &old, _Event const &new);
//           We can provide some common ones:
//           - Throw an error
//           - Use old (ignore)
//           - Use new (overwrite)
//           - OR (operator|)   This should just work, no?
//           - AND (operator&)  This should just work, no?


#include "evt/Event.h"
#include "evt/EventHistory.h"
#include "support/Testing.h"
#include <catch2/catch_test_macros.hpp>


using PostDelayedEvents = evt::detail::PostDelayedEvents;
using PostMultiEvents = evt::detail::PostMultiEvents;
using ResetClockTicks = evt::detail::ResetClockTicks;


struct TestClock {};

struct TestEvent {
  unsigned value;
};

template <>
evt::stage_t constexpr evt::post_stage<TestEvent> = -1;

template <>
struct evt::ClockFor<TestEvent> {
  using value = TestClock;
};


struct TestIdClock { bool id; };

struct TestIdClockedEvent {
  unsigned value;
};

template <>
struct evt::ClockFor<TestIdClockedEvent> {
  using value = TestIdClock;
};


TEST_CASE("Delayed Events") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  testEvents(
    registrations(
      registration<ResetClockTicks>(),
      registration<PostDelayedEvents>(),
      registration<TestEvent>(),
      registration<TestEvent, 0>(evt::all<TestEvent>, evt::all<TestClock>), // Delayed to stage 0 (posts in stage -1)
      registration<TestClock>()
    ),
    step<TestEvent        >(__FILE__, __LINE__, {0}, 0, 0, 1, 0, 0),
    step<ResetClockTicks  >(__FILE__, __LINE__, { }, 1, 0, 1, 0, 0),
    step<TestClock        >(__FILE__, __LINE__, { }, 1, 0, 1, 0, 1),
    step<PostDelayedEvents>(__FILE__, __LINE__, { }, 1, 1, 1, 0, 1),
    step<ResetClockTicks  >(__FILE__, __LINE__, { }, 2, 1, 1, 0, 1),
    step<TestClock        >(__FILE__, __LINE__, { }, 2, 1, 1, 0, 2),
    step<PostDelayedEvents>(__FILE__, __LINE__, { }, 2, 2, 1, 1, 2)
  );
  testEvents(
    registrations(
      registration<ResetClockTicks>(),
      registration<PostDelayedEvents>(),
      registration<TestIdClockedEvent>(),
      registration<TestIdClockedEvent, 1>(evt::all<TestIdClockedEvent>, evt::filter<&TestIdClock::id>(false)), // Delayed to stage 1
      registration<TestIdClock>(evt::filter<&TestIdClock::id>(true)),
      registration<TestIdClock>(evt::filter<&TestIdClock::id>(false))
    ),
    // New Cycle
    step<ResetClockTicks   >(__FILE__, __LINE__, {     }, 1, 0, 0, 0, 0, 0),
    step<TestIdClockedEvent>(__FILE__, __LINE__, {    0}, 1, 0, 1, 0, 0, 0),
    step<TestIdClock       >(__FILE__, __LINE__, { true}, 1, 0, 1, 0, 1, 0),
    step<PostDelayedEvents >(__FILE__, __LINE__, {     }, 1, 1, 1, 0, 1, 0),
    // New Cycle
    step<ResetClockTicks   >(__FILE__, __LINE__, {     }, 2, 1, 1, 0, 1, 0),
    step<TestIdClock       >(__FILE__, __LINE__, { true}, 2, 1, 1, 0, 2, 0),
    step<PostDelayedEvents >(__FILE__, __LINE__, {     }, 2, 2, 1, 0, 2, 0),
    // New Cycle
    step<ResetClockTicks   >(__FILE__, __LINE__, {     }, 3, 2, 1, 0, 2, 0),
    step<TestIdClockedEvent>(__FILE__, __LINE__, {    0}, 3, 2, 2, 0, 2, 0),
    step<TestIdClock       >(__FILE__, __LINE__, {false}, 3, 2, 2, 0, 2, 1),
    step<PostDelayedEvents >(__FILE__, __LINE__, {     }, 3, 3, 2, 0, 2, 1), // here events should be at 0
    // New Cycle
    step<ResetClockTicks   >(__FILE__, __LINE__, {     }, 4, 3, 2, 0, 2, 1), 
    step<TestIdClock       >(__FILE__, __LINE__, { true}, 4, 3, 2, 0, 3, 1),
    step<PostDelayedEvents >(__FILE__, __LINE__, {     }, 4, 4, 2, 0, 3, 1),
    // New Cycle
    step<ResetClockTicks   >(__FILE__, __LINE__, {     }, 5, 4, 2, 0, 3, 1),
    step<TestIdClock       >(__FILE__, __LINE__, {false}, 5, 4, 2, 0, 3, 2), // here events should go to 1
    step<PostDelayedEvents >(__FILE__, __LINE__, {     }, 5, 5, 2, 2, 3, 2)
  );
}


struct TestEventTag {};
struct TestClockTagged {};

template <typename _Event>
struct evt::ClockFor<_Event, std::enable_if_t<std::is_base_of_v<TestEventTag, _Event>>> {
  using value = TestClockTagged;
};

struct TestEventTagged1 : TestEventTag {};
struct TestEventTagged2 : TestEventTag {};


TEST_CASE("Delayed Events with Event Tag") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  testEvents(
    registrations(
      registration<PostDelayedEvents>(),
      registration<TestEvent, evt::post_stage<TestEvent>>(evt::all<TestEvent>, evt::all<TestClock>),
      registration<TestEventTagged1, 0>(evt::all<TestEventTagged1>, evt::all<TestClockTagged>),
      registration<TestEventTagged2, 0>(evt::all<TestEventTagged2>, evt::all<TestClockTagged>),
      registration<TestClock>(),
      registration<TestClockTagged>()
    ),
    step<TestEvent        >(__FILE__, __LINE__, {0}, 0, 0, 0, 0, 0, 0),
    step<TestEventTagged1 >(__FILE__, __LINE__, { }, 0, 0, 0, 0, 0, 0),
    step<TestClock        >(__FILE__, __LINE__, { }, 0, 0, 0, 0, 1, 0),
    step<PostDelayedEvents>(__FILE__, __LINE__, { }, 1, 1, 0, 0, 1, 0),
    step<TestClock        >(__FILE__, __LINE__, { }, 1, 1, 0, 0, 2, 0),
    step<PostDelayedEvents>(__FILE__, __LINE__, { }, 2, 1, 0, 0, 2, 0),
    step<TestClockTagged  >(__FILE__, __LINE__, { }, 2, 1, 0, 0, 2, 1),
    step<PostDelayedEvents>(__FILE__, __LINE__, { }, 3, 1, 1, 0, 2, 1),
    step<TestEventTagged1 >(__FILE__, __LINE__, { }, 3, 1, 1, 0, 2, 1),
    step<TestEventTagged2 >(__FILE__, __LINE__, { }, 3, 1, 1, 0, 2, 1),
    step<TestClock        >(__FILE__, __LINE__, { }, 3, 1, 1, 0, 3, 1),
    step<TestClockTagged  >(__FILE__, __LINE__, { }, 3, 1, 1, 0, 3, 2),
    step<PostDelayedEvents>(__FILE__, __LINE__, { }, 4, 1, 2, 1, 3, 2)
  );
  evt::detail::clearEventData(); 
}


TEST_CASE("Event History without delayed handling or required history depth") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  evt::post(TestEvent{});
  evt::post(TestClock{});
  evt::post(PostDelayedEvents{});
  evt::post(TestEvent{});
  evt::post(TestClock{});
  evt::post(PostDelayedEvents{});
  evt::post(TestEvent{});
  evt::post(TestClock{});
  evt::post(PostDelayedEvents{});
  // There should be no history created at all without any delayed handlers or requireHistory calls.
  REQUIRE(evt::detail::EventHistoryManager<TestEvent>::getEventHistoryManager()->event_history.size() == 0);
  REQUIRE(evt::detail::EventHistoryManager<TestClock>::getEventHistoryManager()->event_history.size() == 0);
  REQUIRE(evt::detail::EventHistoryManager<PostDelayedEvents>::getEventHistoryManager()->event_history.size() == 0);
  evt::detail::clearEventData();
}

TEST_CASE("Event History struct with delayed handling") { //"or required history depth new function""
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  auto test_event_history_0_4 = evt::getHistory<TestEvent>(evt::all<TestEvent>, evt::all<TestClock>, evt::InRange{0, 4});
  CHECK_THROWS(evt::getHistory<TestEvent>(evt::all<TestEvent>, evt::all<TestClock>, evt::InRange{-5, 4})); 
  CHECK_THROWS(evt::getHistory<TestEvent>(evt::all<TestEvent>, evt::all<TestClock>, evt::InRange{9, 4})); 
  auto test_event_history_UpTo3 = evt::getHistory<TestEvent>(evt::all<TestClock>, evt::UpTo{3}); // check these versions with no filter functions work
  auto test_event_history_Last3 = evt::getHistory<TestEvent>(evt::all<TestClock>, evt::Last{3}); // check these versions with no filter functions work
  auto test_event_history_InStage1 = evt::getHistory<TestEvent>(evt::all<TestEvent>, evt::all<TestClock>, evt::InStage{1});

  //everything should be empty
  CHECK(evt::isEmpty(test_event_history_0_4));
  CHECK(evt::isEmpty(test_event_history_UpTo3));
  CHECK(evt::isEmpty(test_event_history_Last3));
  CHECK(evt::isEmpty(test_event_history_InStage1));
  
  CHECK(test_event_history_0_4[0].size()==0);
  CHECK(test_event_history_UpTo3[0].size()==0);
  CHECK(test_event_history_Last3[0].size()==0);
  CHECK_THROWS(test_event_history_InStage1[0].size()==0);

  CHECK(evt::isEmpty(evt::toVector(test_event_history_0_4)));
  CHECK(evt::isEmpty(evt::toVector(test_event_history_UpTo3)));
  CHECK(evt::isEmpty(evt::toVector(test_event_history_Last3)));
  CHECK(evt::isEmpty(evt::toVector(test_event_history_InStage1)));

  CHECK(evt::isEmpty(evt::toTable(test_event_history_0_4)));
  CHECK(evt::isEmpty(evt::toTable(test_event_history_UpTo3)));
  CHECK(evt::isEmpty(evt::toTable(test_event_history_Last3)));
  CHECK(evt::isEmpty(evt::toTable(test_event_history_InStage1)));


  evt::post(TestEvent{}); evt::post(TestClock{}); evt::post(PostDelayedEvents{});
  evt::post(TestEvent{}); evt::post(TestClock{}); evt::post(PostDelayedEvents{});
  evt::post(TestEvent{}); evt::post(TestClock{}); evt::post(PostDelayedEvents{});
  evt::post(TestEvent{}); evt::post(TestClock{}); evt::post(PostDelayedEvents{});
  // There is no history for TestClock or PostDelayedEvents at all.
  REQUIRE(evt::detail::EventHistoryManager<TestClock>::getEventHistoryManager()->event_history.size() == 0);
  REQUIRE(evt::detail::EventHistoryManager<PostDelayedEvents>::getEventHistoryManager()->event_history.size() == 0);
  {
    auto &hist = evt::detail::EventHistoryManager<TestEvent>::getEventHistoryManager()->event_history;
    // There's 1 clock registered.
    REQUIRE(hist.size() == 1);
    auto &stages = std::get<evt::detail::FilteredHandlers<TestEvent>>(hist.back());
    // There's NO event filter registered because requireHistory doesn't use a filter
    REQUIRE(stages.size() == 0);
    auto &events = std::get<evt::detail::StagedEvents<TestEvent>>(hist.back());
    // There are 4 events being maintained (stages -1, 0, 1, 2).
    CHECK(events.size() == 4);

    // Test [] operator for history
    // Returns vector of events - almost equivalent to toTable
    CHECK(test_event_history_0_4[0].size()==1);
    CHECK(test_event_history_0_4[1].size()==1);
    CHECK(test_event_history_0_4[2].size()==1);
    CHECK(test_event_history_0_4[3].size()==0);
    CHECK(test_event_history_0_4[4].size()==0);
    CHECK_THROWS(test_event_history_0_4[5].size()==0);

    // Test isValid functionality
    CHECK(test_event_history_0_4.isValid(0));
    CHECK(test_event_history_0_4.isValid(1));
    CHECK(test_event_history_0_4.isValid(2));
    CHECK(!test_event_history_0_4.isValid(3));
    CHECK(!test_event_history_0_4.isValid(4));
    CHECK(!test_event_history_0_4.isValid(3,6));
    CHECK(!test_event_history_0_4.isValid(-1));
    CHECK(test_event_history_0_4.isValid(0,2));
    CHECK(test_event_history_0_4.isValid<0,2>());
    CHECK(test_event_history_0_4.isValid<1>());

    // Check toVector Functionality

    // Check InRange History
    auto history_vector_0_4 = toVector(test_event_history_0_4);
    auto& test_event_vector_0_4 = history_vector_0_4.data;

    for(auto event: history_vector_0_4){
      if(event!=nullptr){
        // just make sure this code runs
      }
    }

    // We asked for range from 0, 4 so there should be 5 elements here but [3], [4] should be nullptr
    CHECK(test_event_vector_0_4.size() == 5);
    CHECK(std::count(test_event_vector_0_4.begin(), test_event_vector_0_4.end(), nullptr) == 2);
    CHECK_THROWS(history_vector_0_4[-1] == nullptr); // this should throw because range should start at 0
    CHECK(history_vector_0_4[0] != nullptr);
    CHECK(history_vector_0_4[1] != nullptr);
    CHECK(history_vector_0_4[2] != nullptr);
    CHECK(history_vector_0_4[3] == nullptr);
    CHECK(history_vector_0_4[4] == nullptr);
    

    // Check UpTo History
    auto history_vector_UpTo3 = toVector(test_event_history_UpTo3);
    auto& test_event_vector_UpTo3 = history_vector_UpTo3.data;

    // We asked for range UpTo3 so there should be 5 elements [-1, 3] here but [3] should be nullptr
    CHECK(test_event_vector_UpTo3.size() == 5);
    CHECK(std::count(test_event_vector_UpTo3.begin(), test_event_vector_UpTo3.end(), nullptr) == 1);
    CHECK(history_vector_UpTo3[-1] != nullptr);
    CHECK(history_vector_UpTo3[0] != nullptr);
    CHECK(history_vector_UpTo3[1] != nullptr);
    CHECK(history_vector_UpTo3[2] != nullptr);
    CHECK(history_vector_UpTo3[3] == nullptr);


    // Check Last History
    auto history_vector_Last3 = toVector(test_event_history_Last3);
    auto& test_event_vector_Last3 = history_vector_Last3.data;

    // We asked for range Last3 so there should be 3 elements [-1, 2]
    CHECK(test_event_vector_Last3.size() == 3);
    CHECK(std::count(test_event_vector_Last3.begin(), test_event_vector_Last3.end(), nullptr) == 0);
    CHECK(history_vector_Last3[-1] != nullptr);
    CHECK(history_vector_Last3[0] != nullptr);
    CHECK(history_vector_Last3[1] != nullptr);

    CHECK(test_event_history_Last3.isValid(-1));
    CHECK(test_event_history_Last3.isValid(0));
    CHECK(test_event_history_Last3.isValid(1));
    CHECK(test_event_history_Last3.isValid(-1,1));
    CHECK(test_event_history_Last3.isValid<-1,0>());
    CHECK(test_event_history_Last3.isValid<-1,2>());
    CHECK(test_event_history_Last3.isValid<1,3>());

    CHECK(!test_event_history_Last3.isValid<2>());
    CHECK(!test_event_history_Last3.isValid(3));
    CHECK(!test_event_history_Last3.isValid(3,4));

    // Check InStage History
    auto history_vector_Stage1 = toVector(test_event_history_InStage1);
    auto& test_event_vector_Stage1 = history_vector_Stage1.data;

    // We asked for stage 1 so there should be 1 element
    CHECK(test_event_vector_Stage1.size() == 1);
    CHECK(std::count(test_event_vector_Stage1.begin(), test_event_vector_Stage1.end(), nullptr) == 0);
    CHECK_THROWS(history_vector_Stage1[-1] == nullptr);
    CHECK_THROWS(history_vector_Stage1[0] == nullptr);
    CHECK(history_vector_Stage1[1] != nullptr);
    CHECK_THROWS(history_vector_Stage1[2] == nullptr);


    // Check toTable Functionality

    // Check InRange History
    auto history_table_0_4 = toTable(test_event_history_0_4);

    for(auto event_vector: history_table_0_4){
      if(!event_vector.empty()){
        CHECK(event_vector[0] != nullptr);
        // just make sure this code runs
      }
    }

    // We asked for range from 0, 4 so there should be 5 elements here but [3], [4] should be nullptr
    CHECK(history_table_0_4.data.size() == 5);
    CHECK_THROWS(history_table_0_4[-1][0] == nullptr); // this should throw because range should start at 0
    CHECK(history_table_0_4[0][0] != nullptr);
    CHECK(history_table_0_4[1][0] != nullptr);
    CHECK(history_table_0_4[2][0] != nullptr);
    CHECK(history_table_0_4[3].empty());
    CHECK(history_table_0_4[4].empty());


    // Check UpTo History
    auto history_table_UpTo3 = toTable(test_event_history_UpTo3);

    // We asked for range UpTo3 so there should be 5 elements [-1, 3] here but [3] should be nullptr
    CHECK(history_table_UpTo3.data.size() == 5);
    CHECK(history_table_UpTo3[-1][0] != nullptr);
    CHECK(history_table_UpTo3[0][0] != nullptr);
    CHECK(history_table_UpTo3[1][0] != nullptr);
    CHECK(history_table_UpTo3[2][0] != nullptr);
    CHECK(history_table_UpTo3[3].empty());


    // Check Last History
    auto history_table_Last3 = toTable(test_event_history_Last3);

    // We asked for range Last3 so there should be 3 elements [-1, 2]
    CHECK(history_table_Last3.data.size() == 3);
    CHECK(history_table_Last3[-1][0] != nullptr);
    CHECK(history_table_Last3[0][0] != nullptr);
    CHECK(history_table_Last3[1][0] != nullptr);

    // Check InStage History
    auto history_table_Stage1 = toTable(test_event_history_InStage1);

    // We asked for stage 1 so there should be 1 element
    CHECK(history_table_Stage1.data.size() == 1);
    CHECK_THROWS(history_table_Stage1[-1].empty());
    CHECK_THROWS(history_table_Stage1[0].empty());
    CHECK(history_table_Stage1[1][0] != nullptr);
    CHECK_THROWS(history_table_Stage1[2].empty());
  }
  evt::post(TestEvent{}); evt::post(TestClock{});
  {
    auto &hist = evt::detail::EventHistoryManager<TestEvent>::getEventHistoryManager()->event_history;
    // There's 1 clock registered.
    REQUIRE(hist.size() == 1);
    auto &stages = std::get<evt::detail::FilteredHandlers<TestEvent>>(hist.back());
    // There's NO event filter registered because requireHistory doesn't use a filter
    REQUIRE(stages.size() == 0);
    auto &events = std::get<evt::detail::StagedEvents<TestEvent>>(hist.back());
    // There are 5 events being maintained (stages -2, -1, 0, 1, 2).
    CHECK(events.size() == 5);

    // Event history still only gets 5 events, because the new one hasn't been processed yet.
    auto history_vector_UpTo3 = toVector(test_event_history_UpTo3);
    auto& test_event_vector_UpTo3 = history_vector_UpTo3.data;

    // We asked for range UpTo3 so there should be 5 elements [-1, 3] here but [3] should be nullptr
    CHECK(test_event_vector_UpTo3.size() == 5);
    CHECK(std::count(test_event_vector_UpTo3.begin(), test_event_vector_UpTo3.end(), nullptr) == 1);
    CHECK(history_vector_UpTo3[-1] != nullptr);
    CHECK(history_vector_UpTo3[0] != nullptr);
    CHECK(history_vector_UpTo3[1] != nullptr);
    CHECK(history_vector_UpTo3[2] != nullptr);
    CHECK(history_vector_UpTo3[3] == nullptr);
  }
  evt::post(PostDelayedEvents{});
  {
    auto &hist = evt::detail::EventHistoryManager<TestEvent>::getEventHistoryManager()->event_history;
    // There's 1 clock registered.
    REQUIRE(hist.size() == 1);
    auto &stages = std::get<evt::detail::FilteredHandlers<TestEvent>>(hist.back());
    // There's NO event filter registered because requireHistory doesn't use a filter
    REQUIRE(stages.size() == 0);
    auto &events = std::get<evt::detail::StagedEvents<TestEvent>>(hist.back());
    // There are 5 events being maintained (stages -1, 0, 1, 2, 3).
    CHECK(events.size() == 5);

    // To Vector Functionality

    // Check InRange History
    auto history_vector_0_4 = toVector(test_event_history_0_4);
    auto& test_event_vector_0_4 = history_vector_0_4.data;

    // We asked for range from 0, 4 so there should be 5 elements here but  [4] should be nullptr
    CHECK(test_event_vector_0_4.size() == 5);
    CHECK(std::count(test_event_vector_0_4.begin(), test_event_vector_0_4.end(), nullptr) == 1);
    CHECK_THROWS(history_vector_0_4[-1] == nullptr); // this should throw because range should start at 0
    CHECK(history_vector_0_4[0] != nullptr);
    CHECK(history_vector_0_4[1] != nullptr);
    CHECK(history_vector_0_4[2] != nullptr);
    CHECK(history_vector_0_4[3] != nullptr);
    CHECK(history_vector_0_4[4] == nullptr);

    // Check UpTo History
    auto history_vector_UpTo3 = toVector(test_event_history_UpTo3);
    auto& test_event_vector_UpTo3 = history_vector_UpTo3.data;

    // We asked for range UpTo3 so there should be 5 elements [-1, 3] here but [3] should be nullptr
    CHECK(test_event_vector_UpTo3.size() == 5);
    CHECK(std::count(test_event_vector_UpTo3.begin(), test_event_vector_UpTo3.end(), nullptr) == 0);
    CHECK(history_vector_UpTo3[-1] != nullptr);
    CHECK(history_vector_UpTo3[0] != nullptr);
    CHECK(history_vector_UpTo3[1] != nullptr);
    CHECK(history_vector_UpTo3[2] != nullptr);
    CHECK(history_vector_UpTo3[3] != nullptr);

    // Check toTable Functionality

    // Check InRange History
    auto history_table_0_4 = toTable(test_event_history_0_4);

    // We asked for range from 0, 4 so there should be 5 elements here but [3], [4] should be nullptr
    CHECK(history_table_0_4.data.size() == 5);
    CHECK_THROWS(history_table_0_4[-1][0] == nullptr); // this should throw because range should start at 0
    CHECK(history_table_0_4[0][0] != nullptr);
    CHECK(history_table_0_4[1][0] != nullptr);
    CHECK(history_table_0_4[2][0] != nullptr);
    CHECK(history_table_0_4[3][0] != nullptr);
    CHECK(history_table_0_4[4].empty());


    // Check UpTo History
    auto history_table_UpTo3 = toTable(test_event_history_UpTo3);

    // We asked for range UpTo3 so there should be 5 elements [-1, 3] here but [3] should be nullptr
    CHECK(history_table_UpTo3[-1][0] != nullptr);
    CHECK(history_table_UpTo3[0][0] != nullptr);
    CHECK(history_table_UpTo3[1][0] != nullptr);
    CHECK(history_table_UpTo3[2][0] != nullptr);
    CHECK(history_table_UpTo3[3][0] != nullptr);
  }
  evt::post(TestEvent{}); evt::post(TestClock{}); evt::post(TestEvent{});
  {
    auto &hist = evt::detail::EventHistoryManager<TestEvent>::getEventHistoryManager()->event_history;
    // There's 1 clock registered.
    REQUIRE(hist.size() == 1);
    auto &stages = std::get<evt::detail::FilteredHandlers<TestEvent>>(hist.back());
    // There's NO event filter registered because requireHistory doesn't use a filter
    REQUIRE(stages.size() == 0);
    auto &events = std::get<evt::detail::StagedEvents<TestEvent>>(hist.back());
    // There are 6 events being maintained (stages -2, -1, 0, 1, 2, 3).
    // There should be 2 events in -2, so 7 events total
    CHECK(events.size() == 7);

    // Event history still only gets 5 events, because the new one hasn't been processed yet.
    // Check InRange History
    auto history_vector_0_4 = toVector(test_event_history_0_4);
    auto& test_event_vector_0_4 = history_vector_0_4.data;

    CHECK(evt::popcount(history_vector_0_4)==4);

    // We asked for range from 0, 4 so there should be 5 elements here but  [4] should be nullptr
    CHECK(test_event_vector_0_4.size() == 5);
    CHECK(std::count(test_event_vector_0_4.begin(), test_event_vector_0_4.end(), nullptr) == 1);
    CHECK_THROWS(history_vector_0_4[-1] == nullptr); // this should throw because range should start at 0
    CHECK(history_vector_0_4[0] != nullptr);
    CHECK(history_vector_0_4[1] != nullptr);
    CHECK(history_vector_0_4[2] != nullptr);
    CHECK(history_vector_0_4[3] != nullptr);
    CHECK(history_vector_0_4[4] == nullptr);
  }
  evt::post(PostDelayedEvents{});
  evt::post(PostMultiEvents{});
  {
    auto &hist = evt::detail::EventHistoryManager<TestEvent>::getEventHistoryManager()->event_history;
    // There's 1 clock registered.
    REQUIRE(hist.size() == 1);
    auto &stages = std::get<evt::detail::FilteredHandlers<TestEvent>>(hist.back());
    // There's NO event filter registered because requireHistory doesn't use a filter
    REQUIRE(stages.size() == 0);
    auto &events = std::get<evt::detail::StagedEvents<TestEvent>>(hist.back());
    // There are 6 stages being maintained (stages -1, 0, 1, 2, 3, 4).
    // In Stage -1, there should be 2 events - so 7 events total
    CHECK(events.size() == 7);

    // Check toTable Functionality

    // Check InRange History
    auto history_table_0_4 = toTable(test_event_history_0_4);

    // We asked for range from 0, 4 so there should be 5 elements here but [3], [4] should be nullptr
    CHECK(history_table_0_4.data.size() == 5);
    CHECK_THROWS(history_table_0_4[-1][0] == nullptr); // this should throw because range should start at 0
    CHECK(history_table_0_4[0].size() == 1);
    CHECK(history_table_0_4[0][0] != nullptr);
    CHECK(history_table_0_4[1][0] != nullptr);
    CHECK(history_table_0_4[2][0] != nullptr);
    CHECK(history_table_0_4[3][0] != nullptr);
    CHECK(history_table_0_4[4][0] != nullptr);


    // Check UpTo History
    auto history_table_UpTo3 = toTable(test_event_history_UpTo3);
    CHECK(evt::popcount(history_table_UpTo3)==6);

    // We asked for range UpTo3 so there should be 5 elements [-1, 3], stage -1 should have 2 elements
    CHECK(history_table_UpTo3.data.size() == 5);
    CHECK(history_table_UpTo3[-1].size() == 2);
    CHECK(history_table_UpTo3[-1][0] != nullptr);
    CHECK(history_table_UpTo3[-1][1] != nullptr);
    CHECK(history_table_UpTo3[0][0] != nullptr);
    CHECK(history_table_UpTo3[1][0] != nullptr);
    CHECK(history_table_UpTo3[2][0] != nullptr);
    CHECK(history_table_UpTo3[3][0] != nullptr);

    // Check [] operator for multiple events
    CHECK(test_event_history_UpTo3[-1].size() == 2);
  }

  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});

  //everything should be empty again
  CHECK(evt::isEmpty(test_event_history_0_4));
  CHECK(evt::isEmpty(test_event_history_UpTo3));
  CHECK(evt::isEmpty(test_event_history_Last3));
  CHECK(evt::isEmpty(test_event_history_InStage1));
  
  CHECK(evt::isEmpty(evt::toVector(test_event_history_0_4)));
  CHECK(evt::isEmpty(evt::toVector(test_event_history_UpTo3)));
  CHECK(evt::isEmpty(evt::toVector(test_event_history_Last3)));
  CHECK(evt::isEmpty(evt::toVector(test_event_history_InStage1)));

  CHECK(evt::isEmpty(evt::toTable(test_event_history_0_4)));
  CHECK(evt::isEmpty(evt::toTable(test_event_history_UpTo3)));
  CHECK(evt::isEmpty(evt::toTable(test_event_history_Last3)));
  CHECK(evt::isEmpty(evt::toTable(test_event_history_InStage1)));
  
  CHECK(evt::popcount(evt::toVector(test_event_history_0_4))==0);
  CHECK(evt::popcount(evt::toVector(test_event_history_UpTo3))==0);
  CHECK(evt::popcount(evt::toVector(test_event_history_Last3))==0);
  CHECK(evt::popcount(evt::toVector(test_event_history_InStage1))==0);

  CHECK(evt::popcount(evt::toTable(test_event_history_0_4))==0);
  CHECK(evt::popcount(evt::toTable(test_event_history_UpTo3))==0);
  CHECK(evt::popcount(evt::toTable(test_event_history_Last3))==0);
  CHECK(evt::popcount(evt::toTable(test_event_history_InStage1))==0);

  evt::detail::clearEventData();
}

// registerHandlers
// 1. Delayed Handler for EventA with delay N
//    - handler for this one has to post a EventB
// 2. Delayed Handler for EventB with delay M

// Testcase
// evt::post(EventA)
// clock, PostDelayedEvent x (N+M)
// check EventB was posted at N+M
// https:// github.ibm.com/ChipDesignRTX/EventFramework/issues/5

struct TestEventA {
  unsigned value;
};
struct TestEventB {
  unsigned value;
};

template <>
struct evt::ClockFor<TestEventA> {
  using value = TestClock;
};

template <>
struct evt::ClockFor<TestEventB> {
  using value = TestClock;
};

void handleEventA(TestEventA const &inEventA){
  evt::post(TestEventB{});
}

static uint8_t numEventBPosts = 0;
void handleEventB(TestEventB const &inEventB){
  ++numEventBPosts;
}

TEST_CASE("Delayed Event Daisy Chain case A ") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  numEventBPosts = 0;

  evt::registerHandler(evt::handler(&handleEventB), evt::arg(evt::all<TestEventB>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&handleEventA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));

  evt::post(TestEventA{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 0
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 1 -> callback to handleEventA is called -> post(TestEventB) -> TestEventB at stage -1
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
  }
  evt::detail::clearEventData();
  numEventBPosts = 0;
}

TEST_CASE("Delayed Event Daisy Chain case B ") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  numEventBPosts = 0;

  // Switch registration order from previous test - should work the same
  evt::registerHandler(evt::handler(&handleEventA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&handleEventB), evt::arg(evt::all<TestEventB>, evt::all<TestClock>, evt::InStage{1}));

  evt::post(TestEventA{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 0
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 1 -> callback to handleEventA is called -> post(TestEventB) -> TestEventB at stage -1
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
  }
  evt::detail::clearEventData();
  numEventBPosts = 0;
}

struct TestEventC {
  unsigned value;
};
template <>
struct evt::ClockFor<TestEventC> {
  using value = TestClock;
};

void handleEventBAndPostC(TestEventB const &inEventB){
  evt::post(TestEventC{});
  ++numEventBPosts;
}

uint8_t numEventCPosts = 0;
void handleEventC(TestEventC const &inEventC){
  ++numEventCPosts;
}

TEST_CASE("Delayed Event Daisy Chain case C ") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  numEventBPosts = 0;
  numEventCPosts = 0;

  evt::registerHandler(evt::handler(&handleEventA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&handleEventBAndPostC), evt::arg(evt::all<TestEventB>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&handleEventC), evt::arg(evt::all<TestEventC>, evt::all<TestClock>, evt::InStage{1}));

  evt::post(TestEventA{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 0
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 1 -> callback to handleEventA is called -> post(TestEventB) -> TestEventB at stage -1
    CHECK(numEventBPosts == 0);
    CHECK(numEventCPosts == 0);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
    CHECK(numEventCPosts == 0);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
    CHECK(numEventCPosts == 1);
  }
  evt::detail::clearEventData();
  numEventBPosts = 0;
  numEventCPosts = 0;
}

TEST_CASE("Delayed Event Daisy Chain case C.1 ") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  numEventBPosts = 0;
  numEventCPosts = 0;

  // Switch registration order from previous test - should work the same
  evt::registerHandler(evt::handler(&handleEventC), evt::arg(evt::all<TestEventC>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&handleEventA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&handleEventBAndPostC), evt::arg(evt::all<TestEventB>, evt::all<TestClock>, evt::InStage{1}));


  evt::post(TestEventA{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 0
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    // TestEventA at stage 1 -> callback to handleEventA is called -> post(TestEventB) -> TestEventB at stage -1
    CHECK(numEventBPosts == 0);
    CHECK(numEventCPosts == 0);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
    CHECK(numEventCPosts == 0);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numEventBPosts == 1);
    CHECK(numEventCPosts == 1);
  }
  evt::detail::clearEventData();
  numEventBPosts = 0;
  numEventCPosts = 0;
}


TEST_CASE("EventArg") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  SECTION("With filter function") {
    { auto src = evt::arg(evt::filter<&TestEvent::value>(0), evt::all<TestClock>, evt::InRange{2, 5});
      CHECK(src.filter == evt::filter<&TestEvent::value>(0));
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.range.begin == 2);
      CHECK(src.range.end == 5);
    }
    { auto src = evt::arg(evt::filter<&TestEvent::value>(0), evt::all<TestClock>, evt::UpTo{5});
      CHECK(src.filter == evt::filter<&TestEvent::value>(0));
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.range.begin == -1);
      CHECK(src.range.end == 5);
    }
    { auto src = evt::arg(evt::filter<&TestEvent::value>(0), evt::all<TestClock>, evt::Last{5});
      CHECK(src.filter == evt::filter<&TestEvent::value>(0));
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.range.begin == -1);
      CHECK(src.range.end == 3);
    }
    { auto src = evt::arg(evt::filter<&TestEvent::value>(0), evt::all<TestClock>, evt::InStage{5});
      CHECK(src.filter == evt::filter<&TestEvent::value>(0));
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.stage == 5);
    }
    { auto src = evt::arg(evt::filter<&TestEvent::value>(0), evt::all<TestClock>, evt::Current{});
      CHECK(src.filter == evt::filter<&TestEvent::value>(0));
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.stage == -1);
    }
    { auto src = evt::arg<TestEvent>(evt::filter<&TestEvent::value>(0));
      CHECK(src.filter == evt::filter<&TestEvent::value>(0));
    }
  }
  SECTION("Without filter function") {
    { auto src = evt::arg<TestEvent>(evt::all<TestClock>, evt::InRange{2, 5});
      CHECK(src.filter == evt::all<TestEvent>);
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.range.begin == 2);
      CHECK(src.range.end == 5);
    }
    { auto src = evt::arg<TestEvent>(evt::all<TestClock>, evt::UpTo{5});
      CHECK(src.filter == evt::all<TestEvent>);
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.range.begin == -1);
      CHECK(src.range.end == 5);
    }
    { auto src = evt::arg<TestEvent>(evt::all<TestClock>, evt::Last{5});
      CHECK(src.filter == evt::all<TestEvent>);
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.range.begin == -1);
      CHECK(src.range.end == 3);
    }
    { auto src = evt::arg<TestEvent>(evt::all<TestClock>, evt::InStage{5});
      CHECK(src.filter == evt::all<TestEvent>);
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.stage == 5);
    }
    { auto src = evt::arg<TestEvent>(evt::all<TestClock>, evt::Current{});
      CHECK(src.filter == evt::all<TestEvent>);
      CHECK(src.clock_filter == evt::all<TestClock>);
      CHECK(src.stage == -1);
    }
    { auto src = evt::arg<TestEvent>();
      CHECK(src.filter == evt::all<TestEvent>);
    }
  }
  evt::detail::clearEventData();
}

struct TestClockBad {};

static uint8_t numPointerA = 0;
void pointerHandlerA(TestEventA const *inEventA) {
  if (inEventA != nullptr){
    ++numPointerA;
  }
}

static uint8_t numPointerA_2 = 0;
void pointerHandlerA_2(TestEventA const *inEventA) {
  if (inEventA != nullptr) {
    ++numPointerA_2;
  }
}

static uint8_t numVectorA = 0;
void vectorHandlerA(std::vector<TestEventA const *> const &inEventVectorA) {
  if (inEventVectorA.size()) {
    ++numVectorA;
  }
}

static uint8_t numHistoryA = 0;
void historyHandlerA(evt::History<TestEventA> const &inEventHistoryA) {
  if (!evt::isEmpty(inEventHistoryA)) {
    ++numHistoryA;
  }
}

static uint8_t numRefA = 0;
void refHandlerA(TestEventA const &inEventA) {
  ++numRefA;
}

static uint8_t numBasicA = 0;
void basicHandlerA(TestEventA const &inEventA) {
  ++numBasicA;
}

TEST_CASE("DifferentHandlers") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  evt::registerHandler(evt::handler(&pointerHandlerA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&vectorHandlerA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&historyHandlerA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InRange{1,3}));
  evt::registerHandler(evt::handler(&refHandlerA), evt::arg(evt::all<TestEventA>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&basicHandlerA), evt::arg(evt::all<TestEventA>));

  evt::post(TestEventA{});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numPointerA == 0);
    CHECK(numVectorA == 0);
    CHECK(numHistoryA == 0);
    CHECK(numRefA == 0);
    CHECK(numBasicA == 1);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numPointerA == 1);
    CHECK(numVectorA == 1);
    CHECK(numHistoryA == 1);
    CHECK(numRefA == 1);
    CHECK(numBasicA == 1);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numPointerA == 1);
    CHECK(numVectorA == 1);
    CHECK(numHistoryA == 2);
    CHECK(numRefA == 1);
    CHECK(numBasicA == 1);
  }
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{});
  {
    CHECK(numPointerA == 1);
    CHECK(numVectorA == 1);
    CHECK(numHistoryA == 3);
    CHECK(numRefA == 1);
    CHECK(numBasicA == 1);
  }
  evt::detail::clearEventData();
}

struct TestOrderingEvent {
  unsigned value;
};

template <>
struct evt::ClockFor<TestOrderingEvent> {
  using value = TestClock;
};


static bool vectorOrderGood = false;
void orderingVectorHandler(std::vector<TestOrderingEvent const *> const &inEventVector) {
  auto saw_event = std::array<bool, 3>{{false, false, false}};
  bool gotData = false;
  vectorOrderGood = true;
  for (auto event : inEventVector) {
    gotData = true;
    saw_event[event->value] = true;
    if ((saw_event[2] && !saw_event[1]) || (saw_event[2] && !saw_event[0])) {
      vectorOrderGood = false;
    }
    if (saw_event[1] && !saw_event[0]) {
      vectorOrderGood = false;
    }
  }
  if (!gotData) {
    vectorOrderGood = false;
  }
}


static bool history_order_good = false;
void orderingHistoryHandler(evt::History<TestOrderingEvent> const &inEventHistory) {
  history_order_good = true;
  bool gotData = false;
  for (auto stageVector : evt::toTable(inEventHistory)) {
    auto saw_event = std::array<bool, 3>{{false, false, false}};
    for (auto event : stageVector) {
      gotData = true;
      saw_event[event->value] = true;
      if ((saw_event[2] && !saw_event[1]) ||( saw_event[2] && !saw_event[0])) {
        history_order_good = false;
      }
      if (saw_event[1] && !saw_event[0]) {
        history_order_good = false;
      }
    }
  }
  if (!gotData) {
    history_order_good = false;
  }
}

TEST_CASE("Vector Event Ordering") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  evt::registerHandler(evt::handler(&orderingVectorHandler), evt::arg(evt::all<TestOrderingEvent>, evt::all<TestClock>, evt::InStage{1}));
  evt::registerHandler(evt::handler(&orderingHistoryHandler), evt::arg(evt::all<TestOrderingEvent>, evt::all<TestClock>, evt::InRange{1,2}));

  evt::post(TestOrderingEvent{0});
  evt::post(TestOrderingEvent{1});
  evt::post(TestOrderingEvent{2});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{}); // stage 0

  evt::post(TestOrderingEvent{0});
  evt::post(TestOrderingEvent{1});
  evt::post(TestOrderingEvent{2});
  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{}); // stage 1
  {
    CHECK(vectorOrderGood);
    CHECK(history_order_good);
  }

  evt::post(TestClock{}); evt::post(PostDelayedEvents{}); evt::post(PostMultiEvents{}); // stage 2
  {
    CHECK(history_order_good);
  }

  evt::detail::clearEventData();
}
