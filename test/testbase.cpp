
#include <map>
#include <string.h>
#include <testbase.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
#include <v8.h>

using namespace xv8;

// TODO: move to utilities
// DOCUMENTATION NOTE: this stream manipulator only affects the next item:
// std::cout << trimstart::trimstart; std::cout << ", foo"; // no effect
// std::cout << trimstart::trimstart << ", foo"; // result "foo".
namespace trimstart {
  struct trimstart_proxy {
    explicit trimstart_proxy(std::ostream & os):os(os){}
    template<typename Rhs>
    friend std::ostream & operator<<(trimstart_proxy const& q, Rhs const& rhs) {
      return q.os << rhs;
    }
    friend std::ostream & operator<<(trimstart_proxy const& q, std::string const& rhs) {
      const char *s = rhs.c_str();
      while ((q.os.tellp()==static_cast<std::streampos>(0)) && *s && (*s == ' ' || *s == ',')) {
        s++;
      }
      return q.os << s;
    }
    friend std::ostream & operator<<(trimstart_proxy const& q, char const* rhs) {
      while ((q.os.tellp()==static_cast<std::streampos>(0)) && *rhs && (*rhs == ' ' || *rhs == ',')) {
        rhs++;
      }
      return q.os << rhs;
    }
    private: std::ostream & os;
  };
  struct trimstart_creator { } trimstart;
  trimstart_proxy operator<<(std::ostream & os, trimstart_creator) {
    return trimstart_proxy(os);
  }
}

// TODO: move to utilities
struct ltcstr {
  bool operator()(char *a, char *b) {
    return std::strcmp(a, b) < 0;
  }
};

// TODO: move to utilities
void resolveParentPath(std::stringstream &result, const char *path) {
  char *lastslash = ::strrchr(path, '/');
  char *lastbackslash = ::strrchr(path, '/');
  char *last = (lastslash > lastbackslash) ? lastslash : lastbackslash;
  if (last) {
    char saved = *last;
    *last = 0;
    result << path;
    *last = saved;
    result << '/';
  }
  else {
    // There are no slashes so the parent is the working dir.
    result << "./";
  }
}

// TODO: move to utilities
void resolveRelativePath(std::stringstream &result, const char *basepath, const char *relpath) {
  // TODO: check if relpath is absolute and use it alone if so.
  resolveParentPath(result, basepath);
  result << relpath;
}

class JSEvaluator : public Evaluator {
  public:
    bool evaluate(TestDocument &doc, Test &test) {
      XMLCh *JS = xercesc::XMLString::transcode("js");
      xercesc::DOMNodeList *scripts = test.element->getElementsByTagName(JS);
      XMLSize_t numscripts = scripts->getLength();
      XMLSize_t numgoodscripts = 0;
      XMLSize_t numbadscripts = 0;
      for (int i = 0; i < numscripts; i++) {
        xercesc::DOMElement *script = static_cast<xercesc::DOMElement*>(scripts->item(i++));
        xercesc::DOMNode *text = script->getFirstChild();
        if (text) {
          const XMLCh *scripttext = text->getNodeValue();
          char *scriptcstr = xercesc::XMLString::transcode(scripttext);
          std::stringstream funccall;
          funccall << "function __f() { try {" << scriptcstr << "} catch(e) { return false; } } __f();";
          v8::Handle<v8::Script> script = v8::Script::Compile(v8::String::New(funccall.str().c_str()), v8::String::New(doc.doc->path));
          if (!script.IsEmpty()) {
            v8::Handle<v8::Value> result = script->Run();
            if (!result.IsEmpty()) {
              if (result->ToBoolean()->Value()) {
                numgoodscripts++;
              }
              else {
                numbadscripts++;
                test.messages << trimstart::trimstart << ", js#" << i << " => false";
              }
            }
            else {
              numbadscripts++;
              test.messages << trimstart::trimstart << ", js#" << i << " empty return";
            }
          }
          else {
            numbadscripts++;
            test.messages << trimstart::trimstart << ", compiler error in s#" << i;
          }
          xercesc::XMLString::release(&scriptcstr);
        }
      }
      if (numbadscripts > 0) {
        test.messages << trimstart::trimstart << ", " << numbadscripts << " of " << numscripts << " scripts failed";
      }
      xercesc::XMLString::release(&JS);
      return numbadscripts == 0;
    }
};

JSEvaluator js_evaluator;

xv8::Document *__loadDoc(TestDocument &testdoc, xercesc::DOMElement *e, XMLCh *attrname) {
  Document *doc = (Document*)0;
  if (attrname && e->hasAttribute(attrname)) {
    char *href = xercesc::XMLString::transcode(e->getAttribute(attrname));
    std::stringstream resolved;
    resolveRelativePath(resolved, testdoc.doc->path, href);
    doc = Document::load(resolved.str().c_str());
    xercesc::XMLString::release(&href);
  }
  else {
    // TODO: new empty document
  }
  return doc;
}

Test::Test(TestDocument &testdoc, xercesc::DOMElement *e) {
  doc = (Document*)0;
  XMLCh *ID = xercesc::XMLString::transcode("id");
  XMLCh *DOCUMENT = xercesc::XMLString::transcode("document");
  XMLCh *HREF = xercesc::XMLString::transcode("href");
  XMLCh *DEFAULT_DOC_HREF = xercesc::XMLString::transcode("default-document-href");
  xercesc::DOMElement *root = testdoc.doc->dom->getDocument()->getDocumentElement();
  xercesc::DOMNodeList *documents = e->getElementsByTagName(DOCUMENT);
  if (documents->getLength()) {
    xercesc::DOMElement *document = (xercesc::DOMElement*)documents->item(0);
    if (document->hasAttribute(HREF)) {
      doc = __loadDoc(testdoc, document, HREF);
    }
    else {
      // TODO: clone <document> to create a new doc.
    }
  }
  else {
    if (root->hasAttribute(DEFAULT_DOC_HREF)) {
      doc = __loadDoc(testdoc, root, DEFAULT_DOC_HREF);
    }
  }
  element = e;
  id = (char*)0;
  if (e->hasAttribute(ID)) {
    id = xercesc::XMLString::transcode(e->getAttribute(ID));
  }
  else {
    id = xercesc::XMLString::replicate("[unnamed test]");
  }
  xercesc::XMLString::release(&ID);
  xercesc::XMLString::release(&DOCUMENT);
  xercesc::XMLString::release(&HREF);
  xercesc::XMLString::release(&DEFAULT_DOC_HREF);
}

Test::~Test(void) {
  xercesc::XMLString::release(&id);
  Document::release(&doc);
}

TestDocument::TestDocument(const char *path) : doc(Document::load(path)) { }

TestDocument::~TestDocument(void) { ; }

bool __checkPrereq(Test &test, std::map<char *, bool, ltcstr> &test_state, std::stringstream &prereq) {
  if (prereq.str().length() == 0) {
    return true;
  }
  bool rval = true;
  std::map<char *, bool, ltcstr>::iterator i = test_state.find((char*)prereq.str().c_str());
  if (i == test_state.end()) {
    rval = false;
  }
  else {
    rval = i->second;
  }
  if (!rval) {
    test.messages << trimstart::trimstart << ", missing prereq '" << prereq.str() << "'";
  }
  return rval;
}

bool TestDocument::evaluate(Evaluator &evaluator) {
  bool rval = true;
  XMLSize_t numpass = 0;
  XMLSize_t numfail = 0;
  std::map<char *, bool, ltcstr> test_state;
  XMLCh *TEST = xercesc::XMLString::transcode("test");
  XMLCh *PREREQ = xercesc::XMLString::transcode("prereq");
  xercesc::DOMElement *root = doc->dom->getDocument()->getDocumentElement();
  xercesc::DOMNodeList *tests = root->getElementsByTagName(TEST);
  XMLSize_t numtests = tests->getLength();
  std::cout << numtests << " tests found in " << doc->path << std::endl;
  for (int i = 0; i < numtests; i++) {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(tests->item(i));
    Test test(*this, e);
    bool testval = true;
    std::cout << "test " << (i+1) << " of " << numtests << ": " << test.id << "... ";
    if (e->hasAttribute(PREREQ)) {
      char *prereqs = xercesc::XMLString::transcode(e->getAttribute(PREREQ));
      char *scan = prereqs;
      std::stringstream prereq;
      while (char c = *scan++) {
        if (c == ' ' || c == ',') {
          testval = __checkPrereq(test, test_state, prereq) && testval;
          prereq.str("");
        }
        else {
          prereq << c;
        }
      }
      testval = __checkPrereq(test, test_state, prereq) && testval;
      xercesc::XMLString::release(&prereqs);
    }
    if (testval) {
      testval = evaluator.evaluate(*this, test)  && testval;
      testval = js_evaluator.evaluate(*this, test) && testval;
    }
    if (testval) {
      std::cout << "[OK]";
      numpass++;
    }
    else {
      std::cout << "[FAIL] (" << test.messages.str() << ")";
      numfail++;
    }
    std::cout << std::endl;
    test_state[xercesc::XMLString::replicate(test.id)] = testval;
    rval = testval && rval;
  }
  for (std::map<char *, bool, ltcstr>::iterator i = test_state.begin(); i != test_state.end(); i++) {
    xercesc::XMLString::release((char**)&i->first);
  }
  xercesc::XMLString::release(&TEST);
  xercesc::XMLString::release(&PREREQ);
  std::cout << numpass << " of " << numtests << " tests passed, " << numfail << " failed (" << ( ((double)numpass) / ((double)numtests) * 100.0 ) << "%)" << std::endl;
  return rval;
}
