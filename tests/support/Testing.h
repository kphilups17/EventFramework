#pragma once

#include "evt/Event.h"
#include "evt/EventHistory.h"
#include <catch2/catch_test_macros.hpp>


template <typename _Event, typename _Counts>
struct Step {
  using Event = _Event;
  _Event event;
  _Counts counts;
  char const *file;
  std::size_t line;
};

template <typename _Event, typename ..._Counts>
Step<_Event, std::array<std::size_t, sizeof...(_Counts)>> step(char const *file, std::size_t line, _Event const &event, _Counts const &...counts) {
  return { event, {static_cast<std::size_t>(counts)...}, file, line };
}


template <typename _Event, typename _Delay = void>
struct Registration {
  using Event = _Event;
  void registerHandler() { evt::registerHandler(evt::handler(&Registration::handleEvent, this), filter); }
  void handleEvent(_Event const &) { ++count; }
  evt::Filter<_Event> filter;
  std::size_t count {0};
};

template <typename _Event, evt::stage_t _delay>
struct Registration<_Event, std::integral_constant<evt::stage_t, _delay>> {
  using Event = _Event;
  void registerHandler() { evt::registerHandler(evt::handler(&Registration::handleEvent, this), evt::arg(filter, clock_filter, evt::InStage{_delay})); }
  void handleEvent(_Event const &) { ++count; }
  evt::Filter<_Event> filter;
  evt::detail::ClockFilter<_Event> clock_filter;
  std::size_t count {0};
};


template <typename _Event>
Registration<_Event, void> registration(evt::Filter<_Event> const &in_filter = evt::all<_Event>) {
  return {in_filter, 0};
}

template <typename _Event, evt::stage_t _n>
Registration<_Event, std::integral_constant<evt::stage_t, _n>> registration(evt::Filter<_Event> const &in_filter, evt::detail::ClockFilter<_Event> const &in_clock_filter) {
  return {in_filter, in_clock_filter, 0};
}


template <typename ..._Events>
void clearEventData() {
  evt::detail::clearEventData(); 
  evt::detail::delayer_list.clear();
}


template <typename ..._Registrations>
void registerHandlers(_Registrations &...in_registrations) {
  (in_registrations.registerHandler(), ...);
}

template <typename ..._Registrations>
std::array<std::size_t, sizeof...(_Registrations)> getCountsFromRegistrations(_Registrations const &...in_registrations) {
  return {in_registrations.count...};
}

template <typename ..._Registrations>
struct Registrations {
  using Counts = std::array<std::size_t, sizeof...(_Registrations)>;

  std::tuple<_Registrations...> registrations_tuple;

  Registrations(_Registrations const &...in_registrations) : registrations_tuple(in_registrations...) {
     clearEventData<typename _Registrations::Event...>();
    std::apply(registerHandlers<_Registrations...>, registrations_tuple);
  }

  ~Registrations() { clearEventData<_Registrations...>(); }

  Counts getCounts() const {
    return std::apply(getCountsFromRegistrations<_Registrations...>, registrations_tuple);
  }
};


template <typename ..._Registers>
Registrations<_Registers...> registrations(_Registers const &...registers) {
  return { registers... };
}

template <typename _Event, typename _Registrations>
void checkStep(Step<_Event, typename _Registrations::Counts> const &step, _Registrations const &registrations) {
  evt::post(step.event);
  INFO("From " << step.file << ":" << step.line);
  CHECK(registrations.getCounts() == step.counts);
}

template <typename _Registrations, typename ..._Events>
void testEvents(
  _Registrations const &registrations,
  Step<_Events, typename _Registrations::Counts> const &...steps
) {
  // Do the steps, checking as we go
  (checkStep(steps, registrations), ...);
}

