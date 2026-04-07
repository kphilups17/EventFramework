#pragma once

#include "detail/hashing.h" //included to support generic hashing of tuples with hash-able types

#include <cxxabi.h>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>


namespace evt {


template <typename _FunctionArg>
using HandlerBase = std::function<void(_FunctionArg)>; 

// The Handler type.  Handlers hold user-defined callback functions that receive events
// when they are posted.
template <typename _Event>
using Handler = HandlerBase<_Event const &>;


// The Filter type.  Filters hold user-defined predicate functions that are used to
// determine whether the associated handler function should be called with a given
// event.
template <typename _Event>
struct Filter;


// Post an event.
template <typename _Event>
void post(_Event const &);


// Register a handler for an event of type _Event.  The filter function is alwaysTrue<_Event>,
// meaning this handler will be called every time the event is posted.
template <typename _Event>
void registerHandler(Handler<_Event> const &);

// Register a handler for an event of type _Event, behind a Filter predicate.
template <typename _Event>
void registerHandler(Handler<_Event> const &, Filter<_Event> const &);


namespace detail {

template <typename _Event>
std::string inline getEventName() {
  auto name = abi::__cxa_demangle(typeid(_Event).name(), nullptr, nullptr, nullptr);
  std::string tname {};
  if (name) {
    tname = name;
    std::free(name);
  } else {
    tname = typeid(_Event).name();
  }
  return tname; 
}


template <typename _Event>
using Handlers = std::vector<Handler<_Event>>;


// Function type aliases to make it easier to work with function pointers.  Using these aliases
// seems to help some syntax highlighters keep track of everything, especially when function
// pointers are present in templates.

template <typename _Function>
using Function = _Function *;

template <typename _Function, typename _Object>
using MemberFunction = _Function _Object::*;


// Traits type that infers information about a filter type.  Where available it will
// hold the Event type, Object type for a member function or variable, and Value
// type for a member variable.
template <auto, typename=void>
struct FilterTraits;

// Type alias accessors for FilterTraits content

template <auto _filter>
using FilterEvent = typename FilterTraits<_filter>::Event;

template <auto _filter>
using FilterObject = typename FilterTraits<_filter>::Object;

template <auto _filter>
using FilterValue = typename FilterTraits<_filter>::Value;


// A filter function for an arbitrary type that always returns true.  Used to register
// handlers for events that would like to handle all postings of that event.
template <typename _T>
bool inline alwaysTrue(_T const &) { return true; }


} // namespace detail


// Create a handler instance from a function pointer.
template <typename _Event>
Handler<_Event> inline handler(detail::Function<void(_Event const &)>);

// Create a handler instance from a member function pointer and object.
template <typename _Event, typename _Object>
Handler<_Event> inline handler(detail::MemberFunction<void(_Event const &), _Object>, _Object *);


// Create a filter instance from a compile-time-known function pointer.
template <auto _function>
Filter<detail::FilterEvent<_function>> inline filter();

// Create a filter instance from a compile-time-known member function pointer and run-time-known object.
template <auto _function>
Filter<detail::FilterEvent<_function>> inline filter(detail::FilterObject<_function> *);

// Create a filter instance from a compile-time-known member pointer and a run-time-known value.
template <auto _member>
Filter<detail::FilterEvent<_member>> inline filter(detail::FilterValue<_member> const &);


// A common instance of the alwaysTrue filter function for each event type.
template <typename _Event>
Filter<_Event> const inline all = filter<detail::alwaysTrue<_Event>>();


//
//
// Implementation
//
//


namespace detail {


struct HandlerManagerBase;


// Type erased list of pointers to Managers for all our event data i.e. handlers and event_history
std::vector<HandlerManagerBase*> inline handler_managers;


// True if new handler registrations are allowed, false otherwise.  Used by modules
// to require all handler registration occur within module-defined windows.
inline void alwaysAllowHandlerRegistration() {}
inline std::function<void()> tryHandlerRegistration = alwaysAllowHandlerRegistration;


struct HandlerManagerBase {
  HandlerManagerBase() { handler_managers.push_back(this); }
  virtual ~HandlerManagerBase() = default;
  virtual void clear() = 0;
  virtual void clearHistory() = 0;
};

// Added to handler_manager list in Event.h and has it's clear function called
template <typename _Event>
struct HandlerManager : HandlerManagerBase {
  ~HandlerManager() override = default;

  static HandlerManager<_Event> *getHandlerManager() {
    static HandlerManager<_Event> instance {};
    return &instance;
  }

  void clear() override {
    handlers.clear();
  }

  // Does nothing here because there is no history
  void clearHistory() override { return; }

  void registerHandler(Handler<_Event> const &in_handler, Filter<_Event> const &in_filter) {
    tryHandlerRegistration();
    for (auto &item : handlers) {
      if (std::get<Filter<_Event>>(item) == in_filter) {
        std::get<detail::Handlers<_Event>>(item).push_back(in_handler);
        return;
      }
    }
    handlers.push_back({in_filter, {in_handler}});
  }

  std::vector<std::tuple<Filter<_Event>, Handlers<_Event>>> const &getHandlers() const {
    return handlers;
  }

private:
  // The main data structure, a series of filter+handlers pairs.  When an event is posted,
  // if the event passes the filter function, the handlers are called.
  std::vector<std::tuple<Filter<_Event>, Handlers<_Event>>> handlers {};
};

// Goes through type-erased list of handler managers and resets all handlers and event_history to a clean slate
inline void clearHandlers() {
  for (auto handler_manager : handler_managers) {
    handler_manager->clear();
  }
}

// Goes through type-erased list of handler managers and resets all the event_history to a clean slate
inline void clearHandlerHistory() {
  for (auto handler_manager : handler_managers) {
    handler_manager->clearHistory();
  }
}


// Two pointers to the same member function are not guaranteed to have the same value,
// only to compare as equal if they're types are fully resolved.  Because there are
// compiler and platform specific concerns here, duplicating all those algorithms
// would be both brittle and require significant effort.  Instead, we use a proxy to
// identify the functions; the address of a per-function boolean, and require that
// filter functions be compile-time-known so that the proxy template can be specialized.

// Pointers to member values do not have a value at all, they are represented internally
// as an offset from the address of an object instance, but have no common value.  They
// are, however, comparable to each other, which means the compiler can also distinguish
// between them.  That means the proxy approach can also be used to identify unique
// pointer to member.

// The proxy type is a char because char is guaranteed to take up only 1 Byte, so
// we only need 1 Byte per unique member  or member function registered.  Const so
// that it ends up in the read-only data section.
template <auto _function>
char const inline member_proxy = '\0';


// Base class for polymorphic Filter implementations.
template <typename _Event>
struct FilterBase;


// Default template, with no default implementation so we can strictly enforce the type of filter functions.
template <typename _Event, auto _function>
struct FilterImpl;


// Direct comparison of a member variable value.
template <auto, typename=void>
struct FilterValueCompare;


template <typename _Event>
struct FilterBase {
  virtual ~FilterBase() {}
  virtual bool invoke(_Event const &) const = 0;
  virtual FilterBase *newCopy() const = 0;
  virtual std::size_t getFunctionId() const { return 0; }
  virtual std::size_t getObjectId() const { return 0; }
};


template <typename _Event, Function<bool(_Event const &)> _function>
struct FilterImpl<_Event, _function> : FilterBase<_Event> {
  virtual ~FilterImpl() {}
  bool invoke(_Event const &in_event) const override { return _function(in_event); }
  FilterBase<_Event> *newCopy() const override { return new FilterImpl{}; }
  std::size_t getFunctionId() const override { return reinterpret_cast<std::size_t>(_function); }
};

template <typename _Event, typename _Object, MemberFunction<bool(_Event const &), _Object> _function>
struct FilterImpl<_Event, _function> : FilterBase<_Event> {
  _Object *object;
  virtual ~FilterImpl() {}
  bool invoke(_Event const &in_event) const override { return (object->*_function)(in_event); }
  FilterBase<_Event> *newCopy() const override { auto result = new FilterImpl{}; result->object = object; return result; }
  std::size_t getFunctionId() const override { return reinterpret_cast<std::size_t>(&member_proxy<_function>); }
  std::size_t getObjectId() const override { return reinterpret_cast<std::size_t>(object); }
};


template <typename _Event, typename _Value, _Value _Event::*_member>
struct FilterValueCompare<_member, std::enable_if_t<std::is_member_object_pointer_v<decltype(_member)>>> 
: FilterBase<_Event> {
  _Value value;
  virtual ~FilterValueCompare() {}
  bool invoke(_Event const &in_event) const override { return detail::hash(in_event.*_member) == detail::hash(value); }
  FilterBase<_Event> *newCopy() const override { auto result = new FilterValueCompare{}; result->value = value; return result; }
  std::size_t getFunctionId() const override { return reinterpret_cast<std::size_t>(&member_proxy<_member>); }
  std::size_t getObjectId() const override { return detail::hash(value); }
};

template <typename _Event, typename _Value, _Value (_Event::*_member)() const>
struct FilterValueCompare<_member> : FilterBase<_Event> {
  _Value value;
  virtual ~FilterValueCompare() {}
  bool invoke(_Event const &in_event) const override { return detail::hash((in_event.*_member)()) == detail::hash(value); }
  FilterBase<_Event> *newCopy() const override { auto result = new FilterValueCompare{}; result->value = value; return result; }
  std::size_t getFunctionId() const override { return reinterpret_cast<std::size_t>(&member_proxy<_member>); }
  std::size_t getObjectId() const override { return detail::hash(value); }
};


template <typename _Event, typename _Value, _Value (*_function)(_Event const&)>
struct FilterValueCompare<_function> : FilterBase<_Event> {
  _Value value;
  virtual ~FilterValueCompare() {}
  bool invoke(_Event const &in_event) const override { return detail::hash(_function(in_event)) == detail::hash(value); }
  FilterBase<_Event> *newCopy() const override { auto result = new FilterValueCompare{}; result->value = value; return result; }
  std::size_t getFunctionId() const override { return reinterpret_cast<std::size_t>(_function); }
  std::size_t getObjectId() const override { return detail::hash(value); }
};

//FilterTrait description
//is_valid_filter_value is used to indicate whether the function being used  is valid type of filter function
//specifically now just checks if the free and member functions being used are const functions
//there is no functional requirement for const-ness but limits confusion in usage

//Free Functions
template <typename _Event, typename _Value, _Value (*_function)(_Event const&)>
struct FilterTraits<_function> {
  using Event = _Event;
  using Value = _Value;
  static constexpr bool is_valid_filter_value = true;
};

template <typename _Event, typename _Object, MemberFunction<bool(_Event const &), _Object> _function>
struct FilterTraits<_function> {
  using Event = _Event;
  using Value = bool;
  using Object = _Object;
  static constexpr bool is_valid_filter_value = true;
};

//Filter Traits for Member Objects
template <typename _Event, typename _Value, _Value _Event::*_member>
struct FilterTraits<_member, std::enable_if_t<std::is_member_object_pointer_v<decltype(_member)>>> {
  using Event = _Event;
  using Value = _Value;
  static constexpr bool is_valid_filter_value = true;
};

//Filter Traits for Member Functions
//Const
template <typename _Event, typename _Value, _Value (_Event::*_member)() const>
struct FilterTraits<_member> {
  using Event = _Event;
  using Value = _Value;
  static constexpr bool is_valid_filter_value = true;
};

//Non-Const
template <typename _Event, typename _Value, _Value (_Event::*_member)()>
struct FilterTraits<_member> {
  using Event = _Event;
  using Value = _Value;
  static constexpr bool is_valid_filter_value = false;
};


} // namespace detail


template <typename _Event>
struct Filter {
  detail::FilterBase<_Event> *ptr {nullptr};

  Filter(detail::FilterBase<_Event> const &base) : ptr(base.newCopy()) {}
  Filter(Filter const &other) : ptr(other.ptr ? other->newCopy() : nullptr) {}
  Filter(Filter &&other) : ptr(other.ptr) { other.ptr = nullptr; }

  ~Filter() { if (ptr) { delete ptr; } }

  Filter &operator=(Filter const &other) { if (ptr) { delete ptr; } if (other.ptr) { ptr = other->newCopy(); } return *this; }
  Filter &operator=(Filter &&other) { if (ptr) { delete ptr; } if (other.ptr) { ptr = other.ptr; other.ptr = nullptr; } return *this; }

  detail::FilterBase<_Event> *operator->() const { return ptr; }
};


template <typename _Event>
bool operator==(Filter<_Event> const &lhs, Filter<_Event> const &rhs) {
  return
    lhs->getFunctionId() == rhs->getFunctionId() and
    lhs->getObjectId()   == rhs->getObjectId();
}


template <typename _Event>
void post(_Event const &in_event) {
  for (auto &item : detail::HandlerManager<_Event>::getHandlerManager()->getHandlers()) {
    if (std::get<Filter<_Event>>(item)->invoke(in_event)) { 
      for (auto &handler : std::get<detail::Handlers<_Event>>(item)) {
        handler(in_event);
      }
    }
  }
}


template <typename _Event>
void registerHandler(Handler<_Event> const &in_handler) {
  registerHandler(in_handler, all<_Event>);
}


template <typename _Event>
void registerHandler(Handler<_Event> const &in_handler, Filter<_Event> const &in_filter) {
  detail::HandlerManager<_Event>::getHandlerManager()->registerHandler(in_handler, in_filter);
}


template <typename _Event>
Handler<_Event> inline handler(detail::Function<void(_Event const &)> in_function) {
  return {in_function};
}

template <typename _Event, typename _Object>
Handler<_Event> inline handler(detail::MemberFunction<void (_Event const &), _Object> in_function, _Object *in_object) {
  return { [in_object, in_function](_Event const &in_event) { return (in_object->*in_function)(in_event); } };
}


template <auto _function>
Filter<detail::FilterEvent<_function>> inline filter() {
  return detail::FilterImpl<detail::FilterEvent<_function>, _function>{};
}


template <auto _function>
Filter<detail::FilterEvent<_function>> inline filter(detail::FilterObject<_function> *in_object) {
  auto result = detail::FilterImpl<detail::FilterEvent<_function>, _function>{};
  result.object = in_object;
  return result;
}


template <auto _member>
Filter<detail::FilterEvent<_member>> inline filter(detail::FilterValue<_member> const &in_value) {
  static_assert(detail::FilterTraits<_member>::is_valid_filter_value, "Member functions for filter value compare must be const functions");
  auto result = detail::FilterValueCompare<_member>{};
  result.value = in_value;
  return result;
}


} // namespace evt
