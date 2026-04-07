#include "evt/Event.h"
#include "evt/EventHistory.h"
#include "support/Testing.h"
#include <catch2/catch_test_macros.hpp>


// Types of multi-event handler:
// 1. Multiple individual events posted in one cycle
//    void multi_handler(EventA const *, EventB const *);
//    registerHandler(evt::handler(multi_handler), evt::filter<A>(), evt::filter<B>());
//    registerHandler(evt::handler(multi_handler), evt::filter<A, B>());

struct MultiEventClock {};
struct MultiEventA { int id; };
struct MultiEventB { int id; };



template <> struct evt::ClockFor<MultiEventA> { using value = MultiEventClock; };
template <> struct evt::ClockFor<MultiEventB> { using value = MultiEventClock; };

static uint8_t a0_count;
static uint8_t a1_count;
static uint8_t b_count;

void multiHandler(MultiEventA const *a0, std::vector<MultiEventA const *> const &a1, evt::History<MultiEventB> const &b) {
  if (a0) ++a0_count;
  if (!a1.empty()) ++a1_count;
  bool any_bs = false;
  for (auto &b : toTable(b)) {
    if (b.size()) {
      any_bs = true;
      break;
    }
  }
  if (any_bs) ++b_count;
}


TEST_CASE("MultiEventHandler FreeFunction Handler") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  a0_count = 0;
  a1_count = 0;
  b_count = 0;
  evt::registerHandler(
    evt::handler(multiHandler),
    evt::arg(
      evt::filter<&MultiEventA::id>(1),
      evt::all<MultiEventClock>,
      evt::Current{}
    ),
    evt::arg(
      evt::filter<&MultiEventA::id>(1),
      evt::all<MultiEventClock>,
      evt::InStage{1}
    ),
    evt::arg(
      evt::filter<&MultiEventB::id>(1),
      evt::all<MultiEventClock>,
      evt::InRange{0, 1}
    )
  );
  evt::post(MultiEventA{1});
  evt::post(MultiEventB{1});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});

  //Events posted but clock has not ticked so all should be zero
  CHECK(a0_count == 0);
  CHECK(a1_count == 0);
  CHECK(b_count == 0);

  evt::post(MultiEventClock{});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});
  
  //Clock has ticked so we should see a0 and b events, not a1 yet
  CHECK(a0_count == 1);
  CHECK(a1_count == 0);
  CHECK(b_count == 1);

  evt::post(MultiEventClock{});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});

  CHECK(a0_count == 1);
  CHECK(a1_count == 1);
  CHECK(b_count == 2);

  evt::post(MultiEventClock{});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});

  CHECK(a0_count == 1);
  CHECK(a1_count == 1);
  CHECK(b_count == 2);

  evt::detail::clearEventData();
}

static uint8_t a0_member_count;
static uint8_t a1_member_count;
static uint8_t b_member_count;

struct TestHandlerObject {
  void multiHandler(MultiEventA const *a0, std::vector<MultiEventA const *> const &a1, evt::History<MultiEventB> const &b) {
  //void multiHandler(MultiEventA const *a0, std::vector<MultiEventA const*> const& a1, evt::History<MultiEventB> const &b) {
    if (a0) ++a0_member_count;
    if (a1.size()) ++a1_member_count;
    //if (a1.size()) ++a1_member_count;
    bool any_bs = false;
    for (auto &b : toTable(b)) {
      if (b.size()) {
        any_bs = true;
        break;
      }
    }
    if (any_bs) ++b_member_count;
  }
};

TEST_CASE("MultiEventHandler MemberFunction Handler") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  a0_member_count = 0;
  a1_member_count = 0;
  b_member_count = 0;
  TestHandlerObject test_handler_object {};
  evt::registerHandler(
    evt::handler(&TestHandlerObject::multiHandler, &test_handler_object),
    evt::arg(
      evt::filter<&MultiEventA::id>(1),
      evt::all<MultiEventClock>,
      evt::Current{}
    ),
    evt::arg(
      evt::filter<&MultiEventA::id>(1),
      evt::all<MultiEventClock>,
      evt::InStage{1}
    ),
    evt::arg(
      evt::filter<&MultiEventB::id>(1),
      evt::all<MultiEventClock>,
      evt::InRange{0, 1}
    )
  );

  evt::post(MultiEventA{1});
  evt::post(MultiEventB{1});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});

  //Events posted but clock has not ticked so all should be zero
  CHECK(a0_member_count == 0);
  CHECK(a1_member_count == 0);
  CHECK(b_member_count == 0);

  evt::post(MultiEventClock{});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});
  
  //Clock has ticked so we should see a0 and b events, not a1 yet
  CHECK(a0_member_count == 1);
  CHECK(a1_member_count == 0);
  CHECK(b_member_count == 1);

  evt::post(MultiEventClock{});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});

  CHECK(a0_member_count == 1);
  CHECK(a1_member_count == 1);
  CHECK(b_member_count == 2);

  evt::post(MultiEventClock{});
  evt::post(evt::detail::PostDelayedEvents{}); evt::post(evt::detail::PostMultiEvents{});

  CHECK(a0_member_count == 1);
  CHECK(a1_member_count == 1);
  CHECK(b_member_count == 2);

  evt::detail::clearEventData();
}


static uint8_t a_ref_count;

void singleRefMultiHandler(MultiEventA const &a) {
  ++a_ref_count;
}

TEST_CASE("Multi-event registration deferral for single const-reference") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  a_ref_count = 0;
  evt::registerHandler(
    evt::handler(singleRefMultiHandler),
    evt::arg(
      evt::filter<&MultiEventA::id>(1),
      evt::all<MultiEventClock>,
      evt::Current{}
    )
  );
  evt::detail::clearEventData();
}
