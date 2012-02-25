

#ifndef XV8_DOM
#define XV8_DOM

#include <v8.h>
#include <xercesc/dom/DOM.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/sax/HandlerBase.hpp>

namespace xv8 {

  class Document {
    private:
      Document(const char *path);

    public:
      static Document *load(const char *path);
      static void *release(Document **doc);
      ~Document(void);
      const char *path;
      xercesc::XercesDOMParser *dom;
      xercesc::ErrorHandler *err;
      v8::Persistent<v8::Context> context;
      v8::Context::Scope context_scope;
  };

}

#endif /* [XV8_DOM] */
