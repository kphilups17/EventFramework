#include "evt/fusion/EventFrameworkInterface.h"
#include <evt/Event.h>


namespace evt::detail {


EventFrameworkInterface::EventFrameworkInterface(): SimObject(nullptr, "EventFrameworkInterface", "") {
  // Disable handler registration
  tryHandlerRegistration = EventFrameworkInterface::allowHandlerRegistration;
}


EventFrameworkInterface::~EventFrameworkInterface() {
  tryHandlerRegistration = alwaysAllowHandlerRegistration;
}


EventFrameworkInterface *EventFrameworkInterface::getEventFrameworkInterface() {
  static EventFrameworkInterface instance {};
  return &instance;
}


void EventFrameworkInterface::allowHandlerRegistration() {
  if (DpxStageManager::getDpxStageManager()->getExecutingStage()->getName() != "PostEnvironmentInit") {
    throw std::runtime_error("Handler registration is only allowed during PostEnvironmentInit.");
  }
}

// We update gCurrentHistoryCycle every cycle after we run PostDelayedEvents
// So if gCurrentHistoryCycle is less than gCurrentSimCycle we haven't run PostDelayedEvents yet
// If it is equal, than we have run it and are in the "late" stage
bool EventFrameworkInterface::isLate() {
  return evt::detail::gCurrentHistoryCycle >= gCurrentSimCycle;
}


ClockEvent *EventFrameworkInterface::registerSimClockCallback(ClockEvent * inClockPtr) {
  // Simple check that it's a valid pointer is probably smart
  if (inClockPtr) {
    if (std::find(mSimClocks.begin(), mSimClocks.end(), inClockPtr) == mSimClocks.end()) {
      mSimClocks.push_back(inClockPtr);
    }
  } else {
    throw std::runtime_error("nullptr provided as ClockPtr for evt::detail::EventFrameworkInterface::registerSimClockCallback()");
  }
  return inClockPtr;
}


void EventFrameworkInterface::resetClockTicks() {
  // Reset Clock ticks at beginning of each sim cycle so that we can keep clock tick information around
  // all cycle for the Late events
  evt::post(evt::detail::ResetClockTicks {});
}


void EventFrameworkInterface::postDelayedEventEvaluate() {
  // We must post these clock events before posting PostDelayedEvents to ensure ordering
  // PostDelayedEvents will tell library to check if we have seen clocks and then increment stage if so
  for (auto clock : mClocks) {
    if (clock->isActive()) {
      evt::post(simClock);
    }
  }
  evt::post(evt::detail::PostDelayedEvents {});
  evt::detail::gCurrentHistoryCycle = gCurrentSimCycle;
  // Posts that happen in the multi-event callback should be considered late
  // They won't get processed until the next sim-cycle
  evt::post(evt::detail::PostMultiEvents {});
}


// This is needed to be called in TestcaseLevel0End to keep from duplicate handlers and event histories being constructed
// when two environment initializations are done in a single program run
void EventFrameworkInterface::clearEventData() {
  evt::detail::clearEventData();
}

// This is needed to be called in TestcaseLevel1End to keep from histories being sustained into subsequent testcases.
// During TestcaseLevel1 there is no PostEnvironmentInit called again, so we have to leave the handlers in place
void EventFrameworkInterface::clearHistory() {
  evt::detail::clearHistory();
}


// Use a static instance of a dummy struct to set the tryHandlerRegistration at module load time.
struct AllowHandlerRegistration  {
  AllowHandlerRegistration() {
    tryHandlerRegistration = EventFrameworkInterface::allowHandlerRegistration;
    isPostingLate = EventFrameworkInterface::isLate;
  }
  ~AllowHandlerRegistration() {
    tryHandlerRegistration = alwaysAllowHandlerRegistration;
    isPostingLate = alwaysFalse;
  }
};


static AllowHandlerRegistration instantiator {};


} // namespace evt::detail
