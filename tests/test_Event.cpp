#include "evt/Event.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>


struct TestEvent { 
  int id; 

  int getId() const {
    return id; 
  }
};

int getId(TestEvent const &e){
  return e.getId(); 
}

bool static odd(TestEvent const &e) { return e.id & 1; }
bool static even(TestEvent const &e) { return (e.id & 1) == 0; }


struct TestFilterObject {
  bool odd(TestEvent const &e) { return e.id & 1; }
  bool even(TestEvent const &e) { return (e.id & 1) == 0; }
};


TEST_CASE("Event") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  evt::Filter<TestEvent> f1 = evt::filter<&TestEvent::id>(5);
  evt::Filter<TestEvent> f2 = evt::filter<&TestEvent::getId>(5);

  CHECK(f1->getFunctionId() != 0);
  CHECK(f2->getFunctionId() != 0);
  CHECK(f1->getFunctionId() != f2->getFunctionId());
  CHECK(f1->getObjectId() != 0);
  CHECK(f2->getObjectId() != 0);
  CHECK(f1->getObjectId() == f2->getObjectId());

  f2 = evt::filter<getId>(4);
  CHECK(f1->getObjectId() != f2->getObjectId());

  f1 = evt::filter<odd>();
  CHECK(f1->getFunctionId() != f2->getFunctionId());
  CHECK(f1->getObjectId() == 0);

  f2 = evt::filter<odd>();
  CHECK(f1->getFunctionId() != 0);
  CHECK(f2->getFunctionId() != 0);
  CHECK(f1->getObjectId() == 0);
  CHECK(f2->getObjectId() == 0);
  CHECK(f1->getFunctionId() == f2->getFunctionId());

  f1 = evt::filter<even>();
  CHECK(f1->getFunctionId() != f2->getFunctionId());
  CHECK(f1->getObjectId() == 0);

  auto foo_inst_1 = TestFilterObject{};
  auto foo_inst_2 = TestFilterObject{};

  f1 = evt::filter<&TestFilterObject::odd>(&foo_inst_1);
  CHECK(f1->getFunctionId() != 0);
  CHECK(f1->getObjectId() != 0);
  CHECK(f1->getFunctionId() != f2->getFunctionId());
  CHECK(f1->getObjectId() != f2->getObjectId());

  f2 = evt::filter<&TestFilterObject::odd>(&foo_inst_1);
  CHECK(f1->getFunctionId() == f2->getFunctionId());
  CHECK(f1->getObjectId() == f2->getObjectId());

  f2 = evt::filter<&TestFilterObject::odd>(&foo_inst_2);
  CHECK(f1->getFunctionId() == f2->getFunctionId());
  CHECK(f1->getObjectId() != f2->getObjectId());

  f2 = evt::filter<&TestFilterObject::even>(&foo_inst_1);
  CHECK(f1->getFunctionId() != f2->getFunctionId());
  CHECK(f1->getObjectId() == f2->getObjectId());

  f2 = evt::filter<&TestFilterObject::even>(&foo_inst_2);
  CHECK(f1->getFunctionId() != f2->getFunctionId());
  CHECK(f1->getObjectId() != f2->getObjectId());
}


unsigned static handleTestEventCount;
void static handleTestEvent(TestEvent const &) { ++handleTestEventCount; }

unsigned static objectHandleTestEventCount;
struct TestHandlerObject {
  void handleTestEvent(TestEvent const &) { ++objectHandleTestEventCount; }
};


TEST_CASE("registerHandler") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  handleTestEventCount = 0;
  evt::registerHandler({handleTestEvent}, evt::filter<odd>());
  evt::post(TestEvent{0});
  CHECK(handleTestEventCount == 0);
  evt::post(TestEvent{1});
  CHECK(handleTestEventCount == 1);
  evt::detail::clearHandlers();

  handleTestEventCount = 0;
  evt::registerHandler(evt::handler(handleTestEvent));  // TODO: Why does this one need the evt:handler call?
  evt::post(TestEvent{0});
  CHECK(handleTestEventCount == 1);
  evt::post(TestEvent{1});
  CHECK(handleTestEventCount == 2);
  evt::detail::clearHandlers();

  TestHandlerObject test_handler_object {};

  objectHandleTestEventCount = 0;
  evt::registerHandler(evt::handler(&TestHandlerObject::handleTestEvent, &test_handler_object), evt::filter<odd>());
  evt::post(TestEvent{0});
  CHECK(objectHandleTestEventCount == 0);
  evt::post(TestEvent{1});
  CHECK(objectHandleTestEventCount == 1);
  evt::detail::clearHandlers();

  objectHandleTestEventCount = 0;
  evt::registerHandler(evt::handler(&TestHandlerObject::handleTestEvent, &test_handler_object));  // TODO: Why does this one need the evt:handler call?
  evt::post(TestEvent{0});
  CHECK(objectHandleTestEventCount == 1);
  evt::post(TestEvent{1});
  CHECK(objectHandleTestEventCount == 2);
  evt::detail::clearHandlers();
}

unsigned static handleMemberObjectCompareCount;
void static handleMemberObjectCompare(TestEvent const &) { ++handleMemberObjectCompareCount; }

unsigned static handleMemberFunctionCompareCount;
void static handleMemberFunctionCompare(TestEvent const &) { ++handleMemberFunctionCompareCount; }

unsigned static handleFreeFunctionCompareCount;
void static handleFreeFunctionCompare(TestEvent const &) { ++handleFreeFunctionCompareCount; }

TEST_CASE("FilterValueCompare tests") {
  evt::detail::tryHandlerRegistration = evt::detail::alwaysAllowHandlerRegistration;

  handleMemberObjectCompareCount = 0;
  handleMemberFunctionCompareCount = 0;
  handleFreeFunctionCompareCount = 0;
  evt::registerHandler({handleMemberObjectCompare}, evt::filter<&TestEvent::id>(1));
  evt::registerHandler({handleMemberFunctionCompare}, evt::filter<&TestEvent::getId>(2));
  evt::registerHandler({handleFreeFunctionCompare}, evt::filter<getId>(3));

  evt::post(TestEvent{0});
  CHECK(handleMemberObjectCompareCount == 0);
  CHECK(handleMemberFunctionCompareCount == 0);
  CHECK(handleFreeFunctionCompareCount == 0);

  evt::post(TestEvent{1});
  CHECK(handleMemberObjectCompareCount == 1);
  CHECK(handleMemberFunctionCompareCount == 0);
  CHECK(handleFreeFunctionCompareCount == 0);
  
  evt::post(TestEvent{2});
  CHECK(handleMemberObjectCompareCount == 1);
  CHECK(handleMemberFunctionCompareCount == 1);
  CHECK(handleFreeFunctionCompareCount == 0);
  
  evt::post(TestEvent{3});
  CHECK(handleMemberObjectCompareCount == 1);
  CHECK(handleMemberFunctionCompareCount == 1);
  CHECK(handleFreeFunctionCompareCount == 1);

  evt::post(TestEvent{1});
  CHECK(handleMemberObjectCompareCount == 2);
  CHECK(handleMemberFunctionCompareCount == 1);
  CHECK(handleFreeFunctionCompareCount == 1);

  evt::detail::clearHandlers();
}
