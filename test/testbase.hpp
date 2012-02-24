
#ifndef XV8_TEST_BASE
#define XV8_TEST_BASE

#include <xercesc/dom/DOM.hpp>
#include <sstream>
#include <xv8.hpp>

namespace xv8 {

  struct Test;
  struct TestDocument;

  class Evaluator {
    public:
      virtual bool evaluate(TestDocument &doc, Test &test)=0;
  };

  class Test {
    friend class TestDocument;
    public:
      xercesc::DOMElement *element;
      std::stringstream messages;
      Document *doc;
      char *id;
    private:
      Test(TestDocument &testdoc, xercesc::DOMElement *element);
      ~Test(void);
  };

  class TestDocument {
    public:
      TestDocument(const char *path);
      ~TestDocument(void);
      bool evaluate(Evaluator &eval);
      Document *doc;
  };

}

#endif /* [XV8_TEST_BASE] */
