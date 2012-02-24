
#include <testbase.hpp>

using namespace xv8;

class DOMEvaluator : public Evaluator {
  public:
    bool evaluate(TestDocument &doc, Test &test) {
      return true;
    }
};

DOMEvaluator dom_evaluator;

int main(int argc, const char **argv) {
  bool good = true;
  for (int i = 1; i < argc; i++) {
    TestDocument testdoc(argv[i]);
    good = testdoc.evaluate(dom_evaluator) && good;
  }
  return good ? 0 : 1;
}
