
#include <iostream>
#include <string.hpp>
#include <xercesc/util/TransService.hpp>
#include <xercesc/util/PlatformUtils.hpp>

using namespace xv8;

String::String(XMLCh *str) : v8str() {
  this->xstr = str;
  this->type = XV8_STRING_XERCES;
  this->copies = 0;
}

String::String(const XMLCh *str) : v8str() {
  this->xstr = (XMLCh*)str;
  this->type = XV8_STRING_XERCES_CONST;
  this->copies = 0;
}

String::String(v8::Handle<v8::Value> &str) : v8str(str) {
  this->type = XV8_STRING_V8;
  this->copies = 0;
}

String::String(char *str) : v8str() {
  this->cstr = str;
  this->type = XV8_STRING_C;
  this->copies = 0;
}

String::String(const char *str) : v8str() {
  this->cstr = (char*)str;
  this->type = XV8_STRING_C_CONST;
  this->copies = 0;
}

String::~String(void) {
  switch(this->type) {
    case XV8_STRING_XERCES:
      if (this->copies & XV8_STRING_XERCES) {
        // Can't transcode xerces -> xerces.
      }
      if (this->copies & XV8_STRING_V8) {
        // We transcoded xerces-> v8, v8's string class should delete.
        // TODO: verify that.
      }
      if (this->copies & XV8_STRING_C) {
        // We transcoded xerces -> C, release it.
        xercesc::XMLString::release(&this->cstr);
      }
    break;
    case XV8_STRING_XERCES_CONST:
      if (this->copies & XV8_STRING_XERCES) {
        // Can't transcode xerces -> xerces.
      }
      if (this->copies & XV8_STRING_V8) {
        // We transcoded xerces-> v8, v8's string class should delete.
        // TODO: verify that.
      }
      if (this->copies & XV8_STRING_C) {
        // We transcoded xerces -> C, release it.
        xercesc::XMLString::release(&this->cstr);
      }
    break;
    case XV8_STRING_V8:
      if (this->copies & XV8_STRING_XERCES) {
        delete[] this->xstr;
      }
      if (this->copies & XV8_STRING_V8) {
        // Can't transcode v8 -> v8.
      }
      if (this->copies & XV8_STRING_C) {
        // We transcoded v8 -> C, delete it.
        delete[] this->cstr;
      }
    break;
    case XV8_STRING_C:
      if (this->copies & XV8_STRING_XERCES) {
        // We transcode C -> xerces, release it.
        xercesc::XMLString::release(&this->xstr);
      }
      if (this->copies & XV8_STRING_V8) {
        // We transcoded C -> v8, v8's string class should delete.
        // TODO: verify that.
      }
      if (this->copies & XV8_STRING_C) {
        // Can't transcode C -> C.
      }
    break;
    case XV8_STRING_C_CONST:
    break;
  };
}

String::operator XMLCh*() {
  return this->_2x();
}

String::operator const XMLCh*() {
  return (const XMLCh*)this->_2x();
}

String::operator v8::Handle<v8::Value>() {
  return this->_2v8();
}

String::operator char*() {
  return this->_2c();
}

String::operator const char*() {
  return this->_2c();
}

XMLCh* String::_2x() {
  switch(this->type) {
    case XV8_STRING_XERCES: return this->xstr; break;
    case XV8_STRING_XERCES_CONST: return this->xstr; break;
    case XV8_STRING_V8: return this->_v2x(); break;
    case XV8_STRING_C: return this->_c2x(); break;
    case XV8_STRING_C_CONST: return this->_c2x(); break;
  };
}

v8::Handle<v8::Value> &String::_2v8() {
  switch(this->type) {
    case XV8_STRING_XERCES: return this->_x2v(); break;
    case XV8_STRING_XERCES_CONST: return this->_x2v(); break;
    case XV8_STRING_V8: return this->v8str; break;
    case XV8_STRING_C: return this->_c2v(); break;
    case XV8_STRING_C_CONST: return this->_c2v(); break;
  };
}

char *String::_2c() {
  switch(this->type) {
    case XV8_STRING_XERCES: return this->_x2c(); break;
    case XV8_STRING_XERCES_CONST: return this->_x2c(); break;
    case XV8_STRING_V8: return this->_v2c(); break;
    case XV8_STRING_C: return this->cstr; break;
    case XV8_STRING_C_CONST: return this->cstr; break;
  };
}

XMLCh* String::_v2x(void) {
  if (this->copies & XV8_STRING_XERCES) {
    return this->xstr;
  }
  this->copies |= XV8_STRING_XERCES;
  v8::String::Utf8Value v8utf(this->v8str);
  xercesc::XMLTransService::Codes failcodes;
  // TODO: make a single shared transcoder.
  XMLSize_t bytes;
  XMLSize_t len = v8utf.length();
  xercesc::XMLTranscoder *utf8_transcoder = xercesc::XMLPlatformUtils::fgTransService->makeNewTranscoderFor("UTF-8", failcodes, len);
  this->xstr = new XMLCh[len];
  unsigned char charSizes[len];
  // TODO better error checking here
  utf8_transcoder->transcodeFrom((const XMLByte*)*v8utf, (unsigned int)len, (XMLCh*)this->xstr, (unsigned int)len, (unsigned int&)bytes, (unsigned char*)&charSizes);
  return this->xstr;
}

XMLCh* String::_c2x(void) {
  if (this->copies & XV8_STRING_XERCES) {
    return this->xstr;
  }
  this->copies |= XV8_STRING_XERCES;
  this->xstr = xercesc::XMLString::transcode(this->cstr);
  return this->xstr;
}

v8::Handle<v8::Value> &String::_x2v(void) {
  if (this->copies & XV8_STRING_V8) {
    return this->v8str;
  }
  this->copies |= XV8_STRING_V8;
  if (this->xstr) {
    this->v8str = v8::String::New((const uint16_t*)this->xstr);
  }
  else {
    this->v8str = v8::Null();
  }
  return this->v8str;
}

v8::Handle<v8::Value> &String::_c2v(void) {
  if (this->copies & XV8_STRING_V8) {
    return this->v8str;
  }
  this->copies |= XV8_STRING_V8;
  if (this->cstr) {
    this->v8str = v8::String::New(this->cstr);
  }
  else {
    this->v8str = v8::Null();
  }
  return this->v8str;
}

char *String::_x2c(void) {
  if (this->copies & XV8_STRING_C) {
    return this->cstr;
  }
  this->copies |= XV8_STRING_C;
  this->cstr = xercesc::XMLString::transcode(this->xstr);
  return this->cstr;
}

char *String::_v2c(void) {
  if (this->copies & XV8_STRING_C) {
    return this->cstr;
  }
  this->copies |= XV8_STRING_C;
  v8::String::AsciiValue ascii(this->v8str->ToString());
  this->cstr = new char[ascii.length()];
  strcpy(this->cstr, *ascii);
  return this->cstr;
}
