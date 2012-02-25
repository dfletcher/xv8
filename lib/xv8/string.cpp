
#include <iostream>
#include <string.hpp>
#include <xercesc/util/TransService.hpp>
#include <xercesc/util/PlatformUtils.hpp>

using namespace xv8;

xercesc::XMLTranscoder *utf8_transcoder = static_cast<xercesc::XMLTranscoder*>(0);

String::String(XMLCh *str) {
  this->cstr = static_cast<char*>(0);
  this->xstr = str;
  this->type = XV8_STRING_XERCES;
  this->releasexstr = false;
  this->releasecstr = false;
  this->deletexstr = false;
  this->deletecstr = false;
}

String::String(const XMLCh *str) {
  this->cstr = static_cast<char*>(0);
  this->xstr = (XMLCh*)str;
  this->type = XV8_STRING_XERCES;
  this->releasexstr = false;
  this->releasecstr = false;
  this->deletexstr = false;
  this->deletecstr = false;
}

String::String(v8::Handle<v8::Value> &str) {
  _vinit(str);
}

String::String(const v8::Handle<v8::Value> &str) {
  _vinit(str);
}

void String::_vinit(const v8::Handle<v8::Value> &str) {
  this->cstr = static_cast<char*>(0);
  v8::String::Utf8Value v8utf(str);
  xercesc::XMLTransService::Codes failcodes;
  // TODO: make a single shared transcoder.
  XMLSize_t bytes;
  XMLSize_t len = v8utf.length();
  if (!utf8_transcoder) {
    utf8_transcoder = xercesc::XMLPlatformUtils::fgTransService->makeNewTranscoderFor("UTF-8", failcodes, 16*1024);
  }
  this->xstr = new XMLCh[len+1];
  this->xstr[len] = 0;
  unsigned char charSizes[len];
  // TODO better error checking here
  utf8_transcoder->transcodeFrom((XMLByte*)*v8utf, (unsigned int)len, (XMLCh*)this->xstr, (unsigned int)len, (unsigned int&)bytes, (unsigned char*)&charSizes);
  this->type = XV8_STRING_XERCES;
  this->releasexstr = false;
  this->releasecstr = false;
  this->deletexstr = true;
  this->deletecstr = false;
}

String::String(char *str) {
  this->cstr = str;
  this->type = XV8_STRING_C;
  this->releasexstr = false;
  this->releasecstr = false;
  this->deletexstr = false;
  this->deletecstr = false;
}

String::String(const char *str) {
  this->cstr = (char*)str;
  this->type = XV8_STRING_C;
  this->releasexstr = false;
  this->releasecstr = false;
  this->deletexstr = false;
  this->deletecstr = false;
}

String::~String(void) {
  if (deletexstr) {
    delete[] this->xstr;
  }
  else if (releasexstr) {
    xercesc::XMLString::release(&this->xstr);
  }
  if (deletecstr) {
    delete[] this->cstr;
  }
  else if (releasecstr) {
    xercesc::XMLString::release(&this->cstr);
  }
}

String::operator XMLCh*() {
  return this->_2x();
}

String::operator const XMLCh*() {
  return (const XMLCh*)this->_2x();
}

String::operator v8::Handle<v8::Value>() {
  switch(this->type) {
    case XV8_STRING_XERCES: return this->_x2v(); break;
    case XV8_STRING_C: return this->_c2v(); break;
  };
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
    case XV8_STRING_C: return this->_c2x(); break;
  };
}

char *String::_2c() {
  switch(this->type) {
    case XV8_STRING_XERCES: return this->_x2c(); break;
    case XV8_STRING_C: return this->cstr; break;
  };
}

XMLCh* String::_c2x(void) {
  if (!this->xstr) {
    this->xstr = xercesc::XMLString::transcode(this->cstr);
    this->releasexstr = true;
  }
  return this->xstr;
}

v8::Handle<v8::Value> String::_x2v(void) {
  if (this->xstr) {
    return v8::String::New((const uint16_t*)this->xstr);
  }
  else {
    return v8::Null();
  }
}

v8::Handle<v8::Value> String::_c2v(void) {
  if (this->cstr) {
    return v8::String::New((const char*)this->cstr);
  }
  else {
    return v8::Null();
  }
}

char *String::_x2c(void) {
  if (!this->cstr) {
    this->cstr = xercesc::XMLString::transcode(this->xstr);
    this->releasecstr = true;
  }
  return this->cstr;
}
