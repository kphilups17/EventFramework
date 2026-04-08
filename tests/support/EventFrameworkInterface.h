#pragma once

#include "evt/Event.h"
#include "evt/EventHistory.h"

#include <algorithm>
#include <stdexcept>
#include <vector>


namespace evt::detail {

// This file contains an example class for building a link between the EventFramework and an external environment.
// An interface class like this can be used to control:
// - When handler registeration is allowed
// - When to determine if it is "Late" in a cycle (typically after PostDelayedEvent is posted)
// - Posting Clock events if Clocks are managed via other C++ objects
// - When the PostDelayedEvent is posted within a cycle
// - When the PostMultiEvent is posted within a cycle
//

/* Below is an example of what a single simulation cycle may look like with the EventFramework. 
 * Early in the cycle, before PostDelayedEvent is posted, the DUT can be examined and the corresponding events can be posted.
 * In the middle of the cycle, when PostDelayedEvent is posted, we check if Clocks have ticked update the event histories accordingly.
 * After, PostDelayedEvent, the new updated event histories are available for consumption. Also, evt::detail::isPostingLate() should return true. 
 * Any new event posted during the isLate()==true phase, can still trigger callbacks but it will not be clocked through histories until the next sim cycle. 
 * 
 *
 *   ================= SIM CYCLE BEGINS ===============
 *                  --
 *                 {        
 *  isLate==false  {       {Various Event Postings} 
 *                 {
                    --
 *   ================= PostDelayedEvent is posted =====
 *                     +- Check if Clocks have ticked and do delayed callbacks and advances events to next stage
 *                  --
 *                 {        
 *  isLate==true   {      Event history is updated and available for consumption 
 *                 {
                    --
 *   ================= SIM CYCLE ENDS =================
 */

template <typename ClockEvent>
struct EventFrameworkInterface {
  EventFrameworkInterface() {
    // Provide an implementation of evt::detail::tryHandlerRegistration for EventFramework
    tryHandlerRegistration = EventFrameworkInterface::allowHandlerRegistration;
    // Provide a implementation of evt::detail::isPostingLate for EventFramework
    isPostingLate = EventFrameworkInterface::isLate;
  }

  ~EventFrameworkInterface() {
    tryHandlerRegistration = alwaysAllowHandlerRegistration;
    isPostingLate = alwaysFalse;
  }

  // Get a pointer to the singleton instance.
  static EventFrameworkInterface *getEventFrameworkInterface() {
    static EventFrameworkInterface instance {};
    return &instance;
  }

  // Current simulation cycle
  int64_t mCurrentSimCycle = 0;
  
  // Used to keep track of whether we are late in the cycle (i.e. after EventEvaluate)
  // Initialized to -1 to indicate that no cycle has been processed yet
  int64_t mCurrentHistoryCycle = -1;

  // Vector to keep track of which Clocks are running in the simulation
  std::vector<ClockEvent *> mSimClocks;

  ClockEvent *registerSimClockCallback(ClockEvent * inClockPtr) {
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

  void postDelayedEventEvaluate() {
    // Clocks are also events in the EventFramework. 
    // If the clock in your environment need to be checked via methods, it can be posted in a function like this
    // so that the EventFramework can tell the clock has ticked. 
    for (auto clock : mSimClocks) {
      if (clock->isActive()) {
        evt::post(*clock);
      }
    }
    // PostDelayedEvent will be handled by the EventFramework. 
    // It will tell library to check if we have seen clocks and then increment stage of relevant events if so
    evt::post(evt::detail::PostDelayedEvents {});
    mCurrentHistoryCycle = mCurrentSimCycle;
    // Posts that happen in the multi-event callback should be considered late
    // They won't get processed until the next sim-cycle
    evt::post(evt::detail::PostMultiEvents {});
  }

  static void allowHandlerRegistration() {
    // In a real simulation environment, this could be controlled to limit when users can register callbacks.
    // E.g. check if we are too early or too late in the environment loading to register handlers and throw an exception
    // We always allow registeration by default
  }

  static bool isLate() {
    // This function should return true if we after the posting of PostDelayedEvents in a given simcycle.
    // The EventFramework has no knowledge of when a cycle begins and ends, except what can be deduced from this call.
    return getEventFrameworkInterface()->checkLate();
  }

  // We update mCurrentHistoryCycle every cycle after we run PostDelayedEvents
  // So if mCurrentHistoryCycle is less than mCurrentSimCycle we haven't run PostDelayedEvents yet
  // If it is equal, than we have run it and are in the "late" stage
  bool checkLate() const {
    return mCurrentHistoryCycle >= mCurrentSimCycle;
  }

  // Set the current simulation cycle
  void setCurrentSimCycle(int64_t cycle) {
    mCurrentSimCycle = cycle;
  }

  // Get the current simulation cycle
  int64_t getCurrentSimCycle() const {
    return mCurrentSimCycle;
  }

  void resetClockTicks() {
    // Reset Clock ticks at beginning of each sim cycle so that we can keep clock tick information around
    // all cycle for the Late events
    evt::post(evt::detail::ResetClockTicks {});
  }

  // This is needed to be called in TestcaseLevel0End to keep from duplicate handlers and event histories being constructed
  // when two environment initializations are done in a single program run
  void clearEventData() {
    evt::detail::clearEventData();
    mSimClocks.clear();
    mCurrentSimCycle = 0;
    mCurrentHistoryCycle = -1;  // Reset to -1 to indicate no cycle has been processed yet
  }

  // This is needed to be called in TestcaseLevel1End to keep from histories being sustained into subsequent testcases.
  // During TestcaseLevel1 there is no PostEnvironmentInit called again, so we have to leave the handlers in place
  void clearHistory() {
    evt::detail::clearHandlerHistory();
    mCurrentHistoryCycle = -1;  // Reset to -1 to indicate no cycle has been processed yet
  }
};


} // namespace evt::detail
