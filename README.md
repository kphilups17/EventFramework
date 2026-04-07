# Event Framework

A C++17 event posting and handling framework.

## Defining an Event

There are no constraints on the type of an event.  Events should be as simple as possible, while containing a complete picture of the data this event represents.

```cpp
struct MyEvent {
  uint32_t data;
  uint8_t x;
};
```

## Posting an Event

Events can be posted from existing data structures, as the result of function calls, or as temporaries that structure some locally available values.

```cpp
// Existing: MyEvent mEvent;
evt::post(mEvent);

// Function: MyEvent getMyEvent();
evt::post(getMyEvent());

// Temporary
uint32_t data = getData();
uint8_t x = getX();
evt::post(MyEvent{data, x});
```

## Handling an Event

Functions that consume events are event handlers.  Handlers can be any function-like object that accepts a const-reference to an event and has a return value of `void`.

```cpp
void handleMyEvent(MyEvent const &event) {
  std::cout << event << '\n';
}

// Register for all MyEvent postings.
evt::registerHandler(evt::handler(handleMyEvent));


class MyEventHandler {
  void handleMyEvent(MyEvent const &event) {
    std::cout << "MyEventHandler: " << event << '\n';
  }
};

// Register for all MyEvent postings.
MyEventHandler my_event_handler {};
evt::registerHandler(evt::handler(&MyEventHandler::handleMyEvent, &my_event_handler));
```

Event handling can be predicated with a filter.  Filters are function-like objects that also define function and object hash methods so that they can be uniquely identified and compared, which allows the library to do things like predicate collections of handlers behind a single filter call.

The library provides a number of filter types by default, and a base class that can be derived from so that users can write their own filter types.  The default filter types are:
- Free functions: `bool (*)(Event const &)`
- Bound member functions: `bool (Object::*)(Event const &)`
- Member variable pointers: `&Event::field`

```cpp
void handleMyEvent(MyEvent const &event) {
  std::cout << event << '\n';
}

// Free function
bool zeroData(MyEvent const &event) {
  return event.data != 0;
}

evt::registerHandler(evt::handler(handleMyEvent), evt::filter<zeroData>());

// Bound member function
struct UniqueMyEvents {
  std::set<MyEvent> events;

  bool isUnique(MyEvent const &event) {
    return events.find(event) == events.end();
  }
};

UniqueMyEvents events {};
evt::registerHandler(evt::handler(handleMyEvent), evt::filter<&UniqueMyEvents::isUnique>(&events));

// Member variable pointer - handler is called if event.x == 5
evt::registerHandler(evt::handler(handleMyEvent), evt::filter<&MyEvent::x>(5));
```

## Using Event Histories

Event histories allow you to register handlers that are called at a later stage, after a clock event has been posted. This enables time-based event processing and access to historical event data.

### Defining a Clock for an Event

To use event histories, first define a clock type and associate it with your event:

```cpp
// Define a clock type
struct MyClock {};

// Define your event
struct MyEvent {
  uint32_t data;
};

// Associate the clock with the event using ClockFor specialization
template <>
struct evt::ClockFor<MyEvent> {
  using value = MyClock;
};

// Optionally define the stage where the event is posted (default is 0)
template <>
evt::stage_t constexpr evt::post_stage<MyEvent> = -1;
```

### Registering a Delayed Handler

Delayed handlers are called at a specific stage after the clock has ticked. Use `evt::arg()` to specify when the handler should be invoked:

```cpp
void handleDelayedEvent(MyEvent const &event) {
  std::cout << "Delayed: " << event.data << '\n';
}

// Register handler to be called at stage 0 (after clock ticks)
evt::registerHandler(
  evt::handler(handleDelayedEvent),
  evt::arg<MyEvent>(evt::all<MyEvent>, evt::all<MyClock>, evt::InStage{0})
);

// Or use Current to handle at the post_stage
evt::registerHandler(
  evt::handler(handleDelayedEvent),
  evt::arg<MyEvent>(evt::all<MyClock>, evt::Current{})
);
```

### Accessing Event Histories

Event histories provide access to past events within a stage range. Use `evt::getHistory()` to retrieve historical data:

```cpp
// Get history for a specific stage range
auto history = evt::getHistory<MyEvent>(
  evt::all<MyEvent>,
  evt::all<MyClock>,
  evt::InRange{-1, 2}  // From stage -1 to stage 2
);

// Check if an event occurred at a specific stage
if (history.isValid(0)) {
  std::cout << "Event occurred at stage 0\n";
}

// Access events using HistoryVector (one event per stage)
auto vector = history.vector();
if (vector[0]) {
  std::cout << "Stage 0 event data: " << vector[0]->data << '\n';
}

// Access events using HistoryTable (multiple events per stage)
auto table = history.table();
for (auto event_ptr : table[0]) {
  std::cout << "Event: " << event_ptr->data << '\n';
}

// Use Last to get the most recent N stages
auto recent = evt::getHistory<MyEvent>(
  evt::all<MyClock>,
  evt::Last{3}  // Last 3 stages
);
```


# Contributing

We run Catch2 tests to unit test the code

To run tests, create a `build` dir at the top level - `mkdir build`

Change to build dir - `cd build`

Run - `cmake -G Ninja ..` 

Run - `ninja tests`

Run - `./tests` to execute tests


To build debug - `cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja ..`

To build ubsan - `cmake -G Ninja --toolchain ../ubsan.cmake ..`

