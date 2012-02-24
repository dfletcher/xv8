
#ifndef XV8_STRING
#define XV8_STRING

#include <v8.h>
#include <xercesc/dom/DOM.hpp>

namespace xv8 {

  class String {

    public:

      typedef enum {
        XV8_STRING_XERCES = (1 << 0),
        XV8_STRING_XERCES_CONST = (1 << 1),
        XV8_STRING_V8 = (1 << 2),
        XV8_STRING_C = (1 << 3),
        XV8_STRING_C_CONST = (1 << 4)
      } StorageType;

      String(XMLCh *str);
      String(const XMLCh *str);
      String(v8::Handle<v8::Value> &str);
      String(char *str);
      String(const char *str);
      ~String(void);

      operator XMLCh*();
      operator const XMLCh*();
      operator v8::Handle<v8::Value>();
      operator char*();
      operator const char*();

    private:
      unsigned long copies;
      StorageType type;
      XMLCh *xstr;
      v8::Handle<v8::Value> v8str;
      char *cstr;
      XMLCh* _2x(void);
      XMLCh* _v2x(void);
      XMLCh* _c2x(void);
      v8::Handle<v8::Value> &_2v8(void);
      v8::Handle<v8::Value> &_x2v(void);
      v8::Handle<v8::Value> &_c2v(void);
      char *_2c(void);
      char *_x2c(void);
      char *_v2c(void);
  };

}

#endif /* [XV8_STRING] */
