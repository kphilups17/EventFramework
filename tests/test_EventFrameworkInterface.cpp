#include "EventFrameworkInterface.h"
#include "evt/Event.h"
#include "evt/EventHistory.h"
#include <catch2/catch_test_macros.hpp>

// Mock ClockEvent for testing purposes
// This represents a clock event that would be used in a simulation environment
struct MockClockEvent {
  uint64_t tick_count = 0;
  bool active = true;
  
  bool isActive() const { return active; }
  void setActive(bool state) { active = state; }
  void tick() { ++tick_count; }
};

// Another clock type for testing multiple clock types
struct AuxClockEvent {
  int id = 0;
  bool active = false;
  
  bool isActive() const { return active; }
  void setActive(bool state) { active = state; }
};


TEST_CASE("EventFrameworkInterface - Basic Functionality") {
  // Allow handler registration for testing
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  
  SECTION("Singleton pattern works correctly") {
      auto* instance1 = Interface::getEventFrameworkInterface();
      auto* instance2 = Interface::getEventFrameworkInterface();
      
      REQUIRE(instance1 != nullptr);
      REQUIRE(instance1 == instance2);
  }
  
  SECTION("allowHandlerRegistration can be called") {
      // Should not throw in standalone mode
      REQUIRE_NOTHROW(Interface::allowHandlerRegistration());
  }
}


TEST_CASE("EventFrameworkInterface - Clock Registration") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  // Reset global state
  
  
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  
  // Clear any previous state
  interface->clearEventData();
  
  SECTION("Register a single clock") {
      MockClockEvent clock;
      clock.setActive(true);
      
      auto* registered = interface->registerSimClockCallback(&clock);
      
      REQUIRE(registered == &clock);
      REQUIRE(interface->mSimClocks.size() == 1);
      REQUIRE(interface->mSimClocks[0] == &clock);
  }
  
  SECTION("Register multiple clocks") {
      MockClockEvent clock1, clock2, clock3;
      
      interface->registerSimClockCallback(&clock1);
      interface->registerSimClockCallback(&clock2);
      interface->registerSimClockCallback(&clock3);
      
      REQUIRE(interface->mSimClocks.size() == 3);
      REQUIRE(interface->mSimClocks[0] == &clock1);
      REQUIRE(interface->mSimClocks[1] == &clock2);
      REQUIRE(interface->mSimClocks[2] == &clock3);
  }
  
  SECTION("Registering same clock multiple times does not duplicate") {
      MockClockEvent clock;
      
      interface->registerSimClockCallback(&clock);
      interface->registerSimClockCallback(&clock);
      interface->registerSimClockCallback(&clock);
      
      // Should only be registered once
      REQUIRE(interface->mSimClocks.size() == 1);
  }
  
  SECTION("Registering nullptr throws exception") {
      REQUIRE_THROWS_AS(
      interface->registerSimClockCallback(nullptr),
      std::runtime_error
      );
  }
  
  // Cleanup
  interface->clearEventData();
}


TEST_CASE("EventFrameworkInterface - Cycle Management") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  interface->clearEventData();  // This resets mCurrentSimCycle and mCurrentHistoryCycle to 0
  
  SECTION("isLate() tracks cycle state correctly") {
      
      // At start of cycle, not late
      REQUIRE_FALSE(interface->isLate());
      
      // Simulate advancing to late stage
      interface->mCurrentHistoryCycle = interface->getCurrentSimCycle();
      REQUIRE(interface->isLate());
      
      // New cycle starts
      interface->setCurrentSimCycle(1);
      REQUIRE_FALSE(interface->isLate());
      
      // Advance to late stage again
      interface->mCurrentHistoryCycle = interface->getCurrentSimCycle();
      REQUIRE(interface->isLate());
      
      // History cycle ahead (shouldn't happen but test boundary)
      interface->mCurrentHistoryCycle = interface->getCurrentSimCycle() + 1;
      REQUIRE(interface->isLate());
  }
  
  SECTION("resetClockTicks posts ResetClockTicks event") {
      // Track if ResetClockTicks event is posted
      static unsigned resetCount = 0;
      resetCount = 0;
      
      struct ResetHandler {
      static void handle(evt::detail::ResetClockTicks const&) {
          ++resetCount;
      }
      };
      
      evt::registerHandler(evt::handler(ResetHandler::handle));
      
      interface->resetClockTicks();
      
      REQUIRE(resetCount == 1);
      
      interface->resetClockTicks();
      REQUIRE(resetCount == 2);
      
      evt::detail::clearHandlers();
  }
  
  interface->clearEventData();
}


TEST_CASE("EventFrameworkInterface - Delayed Event Processing") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  interface->clearEventData();
  
  // Reset global state
  
  
  
  SECTION("postDelayedEventEvaluate posts events for active clocks") {
      MockClockEvent clock1, clock2, clock3;
      clock1.setActive(true);
      clock2.setActive(false);
      clock3.setActive(true);
      
      interface->registerSimClockCallback(&clock1);
      interface->registerSimClockCallback(&clock2);
      interface->registerSimClockCallback(&clock3);
      
      // Track posted clock events
      static unsigned clockEventCount = 0;
      static unsigned delayedEventCount = 0;
      static unsigned multiEventCount = 0;
      clockEventCount = 0;
      delayedEventCount = 0;
      multiEventCount = 0;
      
      struct ClockHandler {
      static void handle(MockClockEvent const&) { ++clockEventCount; }
      };
      struct DelayedHandler {
      static void handle(evt::detail::PostDelayedEvents const&) { ++delayedEventCount; }
      };
      struct MultiHandler {
      static void handle(evt::detail::PostMultiEvents const&) { ++multiEventCount; }
      };
      
      evt::registerHandler(evt::handler(ClockHandler::handle));
      evt::registerHandler(evt::handler(DelayedHandler::handle));
      evt::registerHandler(evt::handler(MultiHandler::handle));
      
      interface->setCurrentSimCycle(5);
      interface->postDelayedEventEvaluate();
      
      // Should post 2 clock events (clock1 and clock3 are active)
      REQUIRE(clockEventCount == 2);
      
      // Should post PostDelayedEvents once
      REQUIRE(delayedEventCount == 1);
      
      // Should post PostMultiEvents once
      REQUIRE(multiEventCount == 1);
      
      // History cycle should be updated
      REQUIRE(interface->mCurrentHistoryCycle == interface->getCurrentSimCycle());
      REQUIRE(interface->isLate());
      
      evt::detail::clearHandlers();
  }
  
  SECTION("postDelayedEventEvaluate with no active clocks") {
      MockClockEvent clock1, clock2;
      clock1.setActive(false);
      clock2.setActive(false);
      
      interface->registerSimClockCallback(&clock1);
      interface->registerSimClockCallback(&clock2);
      
      static unsigned clockEventCount = 0;
      clockEventCount = 0;
      
      struct ClockHandler {
      static void handle(MockClockEvent const&) { ++clockEventCount; }
      };
      
      evt::registerHandler(evt::handler(ClockHandler::handle));
      
      interface->postDelayedEventEvaluate();
      
      // No clock events should be posted
      REQUIRE(clockEventCount == 0);
      
      evt::detail::clearHandlers();
  }
  
  SECTION("postDelayedEventEvaluate with no registered clocks") {
      static unsigned clockEventCount = 0;
      clockEventCount = 0;
      
      struct ClockHandler {
      static void handle(MockClockEvent const&) { ++clockEventCount; }
      };
      
      evt::registerHandler(evt::handler(ClockHandler::handle));
      
      interface->postDelayedEventEvaluate();
      
      // No clock events should be posted
      REQUIRE(clockEventCount == 0);
      
      evt::detail::clearHandlers();
  }
  
  interface->clearEventData();
}


TEST_CASE("EventFrameworkInterface - Data Cleanup") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  // Reset global state
  
  
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  
  SECTION("clearEventData removes all registered clocks") {
      MockClockEvent clock1, clock2, clock3;
      
      interface->registerSimClockCallback(&clock1);
      interface->registerSimClockCallback(&clock2);
      interface->registerSimClockCallback(&clock3);
      
      REQUIRE(interface->mSimClocks.size() == 3);
      
      interface->clearEventData();
      
      REQUIRE(interface->mSimClocks.empty());
  }
  
  SECTION("clearHistory can be called") {
      MockClockEvent clock;
      interface->registerSimClockCallback(&clock);
      
      interface->mCurrentHistoryCycle = 5;
      
      // Should not throw
      REQUIRE_NOTHROW(interface->clearHistory());
      
      // Clocks should still be registered after clearHistory
      REQUIRE(interface->mSimClocks.size() == 1);
  }
  
  SECTION("Multiple clearEventData calls are safe") {
      MockClockEvent clock;
      interface->registerSimClockCallback(&clock);
      
      interface->clearEventData();
      REQUIRE(interface->mSimClocks.empty());
      
      // Should be safe to call again
      REQUIRE_NOTHROW(interface->clearEventData());
      REQUIRE(interface->mSimClocks.empty());
  }
  
  interface->clearEventData();
}


TEST_CASE("EventFrameworkInterface - Complete Simulation Cycle") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  interface->clearEventData();  // This resets mCurrentSimCycle and mCurrentHistoryCycle to 0
  
  SECTION("Simulate multiple cycles with clock state changes") {
      MockClockEvent mainClock, auxClock;
      mainClock.setActive(true);
      auxClock.setActive(true);
      interface->clearEventData();
      interface->clearHistory();

      interface->resetClockTicks();
      
      interface->registerSimClockCallback(&mainClock);
      interface->registerSimClockCallback(&auxClock);
      
      static unsigned clockEventCount = 0;
      clockEventCount = 0;
      
      struct ClockHandler {
      static void handle(MockClockEvent const&) { ++clockEventCount; }
      };
      
      evt::registerHandler(evt::handler(ClockHandler::handle));
      
      // Cycle 0
      REQUIRE_FALSE(interface->isLate());  // Before postDelayedEventEvaluate, not late
      
      interface->resetClockTicks();
      interface->postDelayedEventEvaluate();
      
      REQUIRE(interface->isLate());  // After postDelayedEventEvaluate, we are late
      REQUIRE(clockEventCount == 2); // Both clocks active
      
      // Cycle 1 - disable auxClock
      interface->setCurrentSimCycle(1);
      REQUIRE_FALSE(interface->isLate());
      
      auxClock.setActive(false);
      clockEventCount = 0;
      
      interface->resetClockTicks();
      interface->postDelayedEventEvaluate();
      
      REQUIRE(interface->isLate());
      REQUIRE(clockEventCount == 1); // Only mainClock active
      
      // Cycle 2 - re-enable auxClock
      interface->setCurrentSimCycle(2);
      auxClock.setActive(true);
      clockEventCount = 0;
      
      interface->resetClockTicks();
      interface->postDelayedEventEvaluate();
      
      REQUIRE(clockEventCount == 2); // Both active again
      
      evt::detail::clearHandlers();
  }
  
  interface->clearEventData();
}


TEST_CASE("EventFrameworkInterface - Multiple Clock Types") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  // Reset global state
  
  
  
  SECTION("Different EventFrameworkInterface instances for different clock types") {
      using MockInterface = evt::detail::EventFrameworkInterface<MockClockEvent>;
      using AuxInterface = evt::detail::EventFrameworkInterface<AuxClockEvent>;
      
      auto* mockInterface = MockInterface::getEventFrameworkInterface();
      auto* auxInterface = AuxInterface::getEventFrameworkInterface();
      
      // Different types should have different instances
      REQUIRE(static_cast<void*>(mockInterface) != static_cast<void*>(auxInterface));
      
      MockClockEvent mockClock;
      AuxClockEvent auxClock;
      
      mockInterface->clearEventData();
      auxInterface->clearEventData();
      
      mockInterface->registerSimClockCallback(&mockClock);
      auxInterface->registerSimClockCallback(&auxClock);
      
      REQUIRE(mockInterface->mSimClocks.size() == 1);
      REQUIRE(auxInterface->mSimClocks.size() == 1);
      
      mockInterface->clearEventData();
      auxInterface->clearEventData();
  }
}


TEST_CASE("EventFrameworkInterface - Edge Cases and Error Handling") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  // Reset global state
  
  
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  interface->clearEventData();
  
  SECTION("Register clock, clear, then register again") {
      MockClockEvent clock;
      
      interface->registerSimClockCallback(&clock);
      REQUIRE(interface->mSimClocks.size() == 1);
      
      interface->clearEventData();
      REQUIRE(interface->mSimClocks.empty());
      
      // Should be able to register again after clearing
      interface->registerSimClockCallback(&clock);
      REQUIRE(interface->mSimClocks.size() == 1);
  }
  
  SECTION("postDelayedEventEvaluate updates gCurrentHistoryCycle correctly") {
      interface->setCurrentSimCycle(10);
      interface->mCurrentHistoryCycle = 5;
      
      interface->postDelayedEventEvaluate();
      
      REQUIRE(interface->mCurrentHistoryCycle == 10);
      REQUIRE(interface->mCurrentHistoryCycle == interface->getCurrentSimCycle());
  }
  
  SECTION("isLate boundary conditions") {
      interface->setCurrentSimCycle(100);
      
      interface->mCurrentHistoryCycle = 99;
      REQUIRE_FALSE(interface->isLate());
      
      interface->mCurrentHistoryCycle = 100;
      REQUIRE(interface->isLate());
      
      interface->mCurrentHistoryCycle = 101;
      REQUIRE(interface->isLate());
  }
  
  interface->clearEventData();
}


TEST_CASE("EventFrameworkInterface - Integration Example") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;
  
  using Interface = evt::detail::EventFrameworkInterface<MockClockEvent>;
  auto* interface = Interface::getEventFrameworkInterface();
  interface->clearEventData();  // This resets mCurrentSimCycle and mCurrentHistoryCycle to 0
  
  SECTION("Complete usage example demonstrating all functions") {
      // Step 1: Create and register clocks
      MockClockEvent systemClock, peripheralClock;
      systemClock.setActive(true);
      peripheralClock.setActive(true);

      interface->clearEventData();
      interface->clearHistory();
      
      auto* reg1 = interface->registerSimClockCallback(&systemClock);
      auto* reg2 = interface->registerSimClockCallback(&peripheralClock);
      
      REQUIRE(reg1 == &systemClock);
      REQUIRE(reg2 == &peripheralClock);
      REQUIRE(interface->mSimClocks.size() == 2);
      
      // Step 2: Set up event handlers
      static unsigned clockTicks = 0;
      static unsigned resetCount = 0;
      clockTicks = 0;
      resetCount = 0;
      
      struct ClockHandler {
      static void handle(MockClockEvent const&) { ++clockTicks; }
      };
      struct ResetHandler {
      static void handle(evt::detail::ResetClockTicks const&) { ++resetCount; }
      };
      
      evt::registerHandler(evt::handler(ClockHandler::handle));
      evt::registerHandler(evt::handler(ResetHandler::handle));

      
      
      // Step 3: Simulate several cycles
      
      for (uint64_t cycle = 0; cycle < 5; ++cycle) {
      interface->setCurrentSimCycle(cycle);
      
      // Start of cycle - should not be late
      REQUIRE_FALSE(interface->isLate());
      
      // Reset clock ticks
      interface->resetClockTicks();
      
      // Process delayed events
      interface->postDelayedEventEvaluate();
      
      // Now should be late
      REQUIRE(interface->isLate());
      REQUIRE(interface->mCurrentHistoryCycle == cycle);
      }
      
      // Verify events were posted correctly
      REQUIRE(clockTicks == 10); // 2 clocks * 5 cycles
      REQUIRE(resetCount == 5);  // 5 cycles
      
      // Step 4: Disable one clock and continue
      peripheralClock.setActive(false);
      clockTicks = 0;
      
      interface->setCurrentSimCycle(5);
      interface->resetClockTicks();
      interface->postDelayedEventEvaluate();
      
      REQUIRE(clockTicks == 1); // Only systemClock active
      
      // Step 5: Clear history (keeps handlers and clocks)
      interface->clearHistory();
      REQUIRE(interface->mSimClocks.size() == 2);
      
      // Step 6: Complete cleanup
      interface->clearEventData();
      REQUIRE(interface->mSimClocks.empty());
      
      evt::detail::clearHandlers();
  }
}

// Made with Bob
