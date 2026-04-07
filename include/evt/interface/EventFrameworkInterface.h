#pragma once

#include "evt/Event.h"
#include "evt/EventHistory.h"

#include <map>


uint64_t extern gCurrentSimCycle;

namespace evt::detail {

// Used to keep track of whether we are late in the cycle (i.e. after EventEvaluate)
inline uint64_t gCurrentHistoryCycle = 0;

// This file contains the link between the EventFramework and an external environment.
// When histories are registered, they can be registered by passing in a HierarchicalObject, ClockName and a ClockReplicaId.
// The singleton EventFrameworkInterface, will accept these arguments and use the ObjectPtr class to create a binding to the
// correct Fusion clock. EventFrameworkInterface saves a list of pointers to these clocks. During the DelayedEventStage (a stage
// added by the .C), EventFrameworkInterface checks if these clocks are active and then posts their pointers if so. Events that
// wish to use these clocks should be type-annotated with a Clock of type `SimBaseClock*`. Events with histories will also
// have already setup clock callbacks to be called on their specific instance of "SimBaseClock*" (matching done by
// SimClockCompare filter). The clock callbacks will allow the histories to be advanced.
//
// We've decided to limit the "clocks" to have to be of type SimBaseClock for now. We don't want arbitrary events to be
// clocks because we can't guaruntee they will evaluate before DelayedEventStage.
//
// FusionClockHandler will also post the "PostDelayedEvents" event which is required for the event history framework to
// work correctly.

template <typenmae ClockEvent>
struct EventFrameworkInterface {
  EventFrameworkInterface();
  ~EventFrameworkInterface() override;

  // Get a pointer to the singleton instance.
  static EventFrameworkInterface *getEventFrameworkInterface();

  static void allowHandlerRegistration();

  // We update gCurrentHistoryCycle every cycle after we run PostDelayedEvents
  // So if gCurrentHistoryCycle is less than gCurrentSimCycle we haven't run PostDelayedEvents yet
  // If it is equal, than we have run it and are in the "late" stage
  static bool isLate();

  ClockEvent *registerSimClockCallback(ClockEvent * inClockPtr);

  void resetClockTicks();

  void postDelayedEventEvaluate();

  // This is needed to be called in TestcaseLevel0End to keep from duplicate handlers and event histories being constructed
  // when two environment initializations are done in a single program run
  void clearEventData();

  // This is needed to be called in TestcaseLevel1End to keep from histories being sustained into subsequent testcases.
  // During TestcaseLevel1 there is no PostEnvironmentInit called again, so we have to leave the handlers in place
  void clearHistory();

  // Vector to keep track of which SimBaseClocks are running in the simulation
  std::vector<ClockEvent *> mSimClocks;
};


} // namespace evt::detail
