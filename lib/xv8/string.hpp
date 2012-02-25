
#ifndef XV8_STRING
#define XV8_STRING

#include <v8.h>
#include <xercesc/dom/DOM.hpp>

namespace xv8 {

  class String {

    public:

      typedef enum {
        XV8_STRING_XERCES = (1 << 0),
        XV8_STRING_C = (1 << 1),
      } StorageType;

      String(XMLCh *str);
      String(const XMLCh *str);
      String(v8::Handle<v8::Value> &str);
      String(const v8::Handle<v8::Value> &str);
      String(char *str);
      String(const char *str);
      ~String(void);

      operator XMLCh*();
      operator const XMLCh*();
      operator v8::Handle<v8::Value>();
      operator char*();
      operator const char*();

    private:
      bool releasexstr, releasecstr;
      bool deletexstr, deletecstr;
      StorageType type;
      XMLCh *xstr;
      char *cstr;
      XMLCh* _2x(void);
      XMLCh* _c2x(void);
      void _vinit(const v8::Handle<v8::Value> &str);
      v8::Handle<v8::Value> _x2v(void);
      v8::Handle<v8::Value> _c2v(void);
      char *_2c(void);
      char *_x2c(void);
  };

}

#endif /* [XV8_STRING] */
