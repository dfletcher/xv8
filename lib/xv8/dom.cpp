#include <map>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <xv8.hpp>
#include <stdio.h>
#include <errno.h>
#include <xercesc/util/PlatformUtils.hpp>
#include "string.hpp"
#include "config.h"

using namespace v8;
using namespace xv8;

#define XV8_NODE_JS_WRAPPERKEY  "--xv8.node.wrapper--"
#define XV8_NODE_JS_USERDATAKEY "--xv8.userdata.wrapper--"
#define XV8_TOKEN_NODE_USERDATA "::xv8::userdata"
#define XV8_TOKEN_LENGTH        16
#define XV8_TOKEN_ASCII_START   'A'
#define XV8_TOKEN_ASCII_END     'z'

typedef enum {
  XV8_NODE_WRAPPER=1,
  XV8_NAMED_NODE_MAP,
  XV8_NODE_LIST,
  XV8_TYPE_INFO,
  XV8_IMPLEMENTATION,
  XV8_CONFIGURATION,
  XV8_STRING_LIST,
  XV8_ERROR,
  XV8_LOCATION
} NativeType;

Handle<Value> _jsCreateNode(xercesc::DOMNode *xmlnode);
Handle<Value> _jsCreateDOMNodeList(xercesc::DOMNodeList*);
Handle<Value> _jsCreateDOMNamedNodeMap(xercesc::DOMNamedNodeMap*);
Handle<Value> _jsCreateDOMImplementation(xercesc::DOMImplementation*);
Handle<Value> _jsCreateDOMConfiguration(xercesc::DOMConfiguration*);
Handle<Value> _jsCreateDOMTypeInfo(const xercesc::DOMTypeInfo*);
#if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
Handle<Value> _jsCreateDOMStringList(const xercesc::DOMStringList*);
#endif
Handle<Value> _jsCreateDOMError(const xercesc::DOMError*);
Handle<Value> _jsCreateDOMLocator(const xercesc::DOMLocator*);

class ArgumentUnwrapException : public std::exception { ; };

class NodeWrapperUserDataHandler : public xercesc::DOMUserDataHandler {
  public:
    void handle (xercesc::DOMUserDataHandler::DOMOperationType operation, const XMLCh* key, void *data, const xercesc::DOMNode *src, DOMUSERDATAHANDLER_ARG5_CONST xercesc::DOMNode *dst) {
      // TODO: delete user data
    }
};

class NodeWrapper {
  public:
    xercesc::DOMNode *node;
    NodeWrapper(xercesc::DOMNode *n) {
      node = n;
    }
    ~NodeWrapper() {
      if (node->getParentNode() != static_cast<xercesc::DOMNode*>(0)) {
        // The node is in a document tree, so we are not responsible for it.
        return;
      }
      // Since the node does not have a parent, it is either (a) a DOMDocument
      // or (b) orphaned. These cases are different.
      if (node->getNodeType() == xercesc::DOMNode::DOCUMENT_NODE) {
        // Okay it's a document node. See if there are other references to this
        // document somewhere else. If not, it's time to delete it.
        //if (HUD::isLiveDocument(static_cast<DOMDocument*>(node))) {
          //return;
        //}
        // TODO!!!!!! ^^^
      }
      else {
        // It is not a document node and has no parent. Fair game for deletion.
      }
      // If we got this far, the node is going to be deleted. First though,
      // remove any descendant nodes that have a NodeWrapper in them. These
      // nodes still have live references in Javascript and must not be
      // deleted along with this->node ancestor.
      _removeWrappedDescendants(node);
      node->release();
    }
  private:
    void _removeWrappedDescendants(xercesc::DOMNode *n) {
      XMLCh *WRAPPERKEY = xercesc::XMLString::transcode(XV8_NODE_JS_WRAPPERKEY);
      NodeWrapper *rval = static_cast<NodeWrapper*>(n->getUserData(WRAPPERKEY));
      if (rval) {
        // n has a wrapper on it so remove it from the tree.
        xercesc::DOMNode *parent = n->getParentNode();
        if (parent) {
          parent->removeChild(n);
        }
        else {
          // This could happen if the top n being passed into this recursive
          // function still has WRAPPERKEY user data on it. However, since it
          // has no parent we don't need to remove anything.
        }
      }
      else {
        // n does not have a wrapper. recurse, check descendants.
        xercesc::DOMNodeList *children = n->getChildNodes();
        XMLSize_t len = children->getLength();
        for (XMLSize_t i = 0; i < len; i++) {
          _removeWrappedDescendants(children->item(i));
        }
      }
      xercesc::XMLString::release(&WRAPPERKEY);
    }
};

char makeTokenC() {
  return (rand() % (XV8_TOKEN_ASCII_END-XV8_TOKEN_ASCII_START)) + XV8_TOKEN_ASCII_START;
}

const char *makeToken() {
  char *rval = new char[XV8_TOKEN_LENGTH+1];
  for (size_t i = 0; i < XV8_TOKEN_LENGTH; i++) {
    rval[i] = makeTokenC();
  }
  rval[XV8_TOKEN_LENGTH] = 0;
  return (const char *)rval;
}

const char *nodeToken(xercesc::DOMNode *n) {
  XMLCh *xkey = xercesc::XMLString::transcode(XV8_TOKEN_NODE_USERDATA);
  const char *token = static_cast<const char*>(n->getUserData(xkey));
  if (!token) {
    token = makeToken();
    n->setUserData((const XMLCh*)xkey, (void*)token, NULL); // TODO: callback that deletes the hidden global and the token
  }
  xercesc::XMLString::release(&xkey);
  return token;
}

Handle<Object> nodeObject(xercesc::DOMNode *n, const char *name, Handle<FunctionTemplate> proto = FunctionTemplate::New()) {
  HandleScope handle_scope;
  // lookup nodetoken hidden global object
  const char *nodetoken = nodeToken(n);
  Local<v8::String> jsnodetoken = v8::String::New(nodetoken);
  Local<Context> cxt = Context::GetEntered();
  Local<Value> tokenvalue = cxt->Global()->GetHiddenValue(jsnodetoken);
  if (tokenvalue.IsEmpty()) {
    Handle<FunctionTemplate> tokenvalueproto = FunctionTemplate::New();
    tokenvalue = tokenvalueproto->GetFunction()->NewInstance();
    cxt->Global()->SetHiddenValue(jsnodetoken, tokenvalue);
  }
  Local<Object> tokenobject = Local<Object>::Cast(tokenvalue);
  // lookup and return tokenobject[name], creating it if doesn't exist
  Local<v8::String> jsname = v8::String::New(name);
  Local<Value> nodeobj;
  if (tokenobject->Has(jsname)) {
    nodeobj = tokenobject->Get(jsname);
  }
  else {
    nodeobj = proto->GetFunction()->NewInstance();
    tokenobject->Set(jsname, nodeobj);
  }
  return Local<Object>::Cast(handle_scope.Close(nodeobj));
}

#if defined (DOM3)
bool _domIsxxxParam(const char *testlist[], XMLCh *name) {
  for (int i = 0; testlist[i]; i++) {
    XMLCh *test = xercesc::XMLString::transcode(testlist[i]);
    if (xercesc::XMLString::compareIString(name, test) == 0) {
      xercesc::XMLString::release(&test);
      return true;
    }
    xercesc::XMLString::release(&test);
  }
  return false;
}

bool _domIsDOMErrorHandlerParam(XMLCh *name) {
  const char *errhandlertypes[] = { "error-handler", 0 };
  return _domIsxxxParam(errhandlertypes, name);
}

bool _domIsStringParam(XMLCh *name) {
  const char *stringtypes[] = { "schema-type", "schema-location", 0 };
  return _domIsxxxParam(stringtypes, name);
}

bool _domIsBooleanParam(XMLCh *name) {
  const char *booltypes[] = { "canonical-form", "cdata-sections", "comments", "datatype-normalization", "discard-default-content", "entities", "infoset", "namespaces", "namespace-declarations", "normalize-characters", "split-cdata-sections", "validate", "validate-if-schema", "element-content-whitespace", 0 };
  return _domIsxxxParam(booltypes, name);
}

class JSDOMErrorHandler : public xercesc::DOMErrorHandler {
  private:
    bool call(Handle<Value> &funcval, Handle<Object> &self, const xercesc::DOMError &domError) {
      Handle<Function> func = funcval.As<Function>();
      Handle<Value> argv[1];
      argv[0] = _jsCreateDOMError(&domError); // NOTE: domError won't exist after call(). Find a safe way to deal with it so nobody can store a reference (copy it?)
      return func->Call(self, 1, argv)->ToBoolean()->Value();
    }
  public:
    Persistent<Value> callback;
    JSDOMErrorHandler(Local<Value> cb) : callback(cb) { ; }
    bool handleError (const xercesc::DOMError &domError) {
      if (callback->IsFunction()) {
        Handle<Object> self(callback.As<Object>());
        return call(callback, self, domError);
      }
      else if (callback->IsObject()) {
        Local<Value> func = callback->ToObject()->Get(v8::String::New("handleError"));
        if (func->IsFunction()) {
          Handle<Object> self(callback.As<Object>());
          return call(func, self, domError);
        }
        else {
          return func->ToBoolean()->Value();
        }
      }
      else {
        return callback->ToBoolean()->Value();
      }
    }
};
#endif

/** Caller should not delete. */
NodeWrapper *_wrapNode(xercesc::DOMNode *n) {
  XMLCh *WRAPPERKEY = xercesc::XMLString::transcode(XV8_NODE_JS_WRAPPERKEY);
  NodeWrapper *rval = static_cast<NodeWrapper*>(n->getUserData(WRAPPERKEY));
  if (!rval) {
    rval = new NodeWrapper(n);
    n->setUserData(WRAPPERKEY, rval, new NodeWrapperUserDataHandler);
  }
  xercesc::XMLString::release(&WRAPPERKEY);
  return rval;
}

void _jsNodeWeakRefCallback(Persistent< Value > object, void *parameter) {
  NodeWrapper *wrapper = static_cast<NodeWrapper*>(parameter);
  std::cout << "garbage collect node type " << wrapper->node->getNodeType() << std::endl;
}

void _jsNative(Local<Object> jsnode, Handle<Value> native, NativeType t) {
  jsnode->SetInternalField(0, native);
  jsnode->SetInternalField(1, Integer::New(t));
}

bool _jsCheckNative(Local<Object> obj, NativeType t) {
  return (obj->InternalFieldCount() == 2) && (obj->GetInternalField(1)->ToInteger()->Value() == t);
}

void _jsNode(Local<Object> jsnode, xercesc::DOMNode *xmlnode) {
  NodeWrapper *wrapper = new NodeWrapper(xmlnode);
  Persistent<External> weakref(External::New(wrapper));
  weakref.MakeWeak(wrapper, _jsNodeWeakRefCallback);
  _jsNative(jsnode, weakref, XV8_NODE_WRAPPER);
}

xercesc::DOMNode *_jsGetNode(Local<Object> jsnode) {
  Local<External> wrap = Local<External>::Cast(jsnode->GetInternalField(0));
  NodeWrapper *wrapper = static_cast<NodeWrapper*>(wrap->Value());
  return wrapper->node;
}

xercesc::DOMNode *_jsUnwrapNode(Local<Object> obj, xercesc::DOMNode::NodeType type = static_cast<xercesc::DOMNode::NodeType>(0)) {
  if (!_jsCheckNative(obj, XV8_NODE_WRAPPER)) {
    ThrowException(Exception::Error(v8::String::New("expected node wrapper")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  xercesc::DOMNode *node = _jsGetNode(obj);
  if (type) {
    if (node->getNodeType() != type) {
      ThrowException(Exception::Error(v8::String::New("incorrect node type")));
      throw ArgumentUnwrapException();
    }
  }
  return node;
}

/**
 *  DOMCharacterData gets it's own special unwrap function (aside from
 *  _jsUnwrapNode) because several node types can be character data, so
 *  this function specifically checks for them whereas _jsUnwrapNode can
 *  only check for a single nodeType.
 */
xercesc::DOMCharacterData *_jsUnwrapCharacterDataNode(Local<Object> obj) {
  xercesc::DOMNode *node = _jsUnwrapNode(obj);
  if (!node) {
    ThrowException(Exception::Error(v8::String::New("NULL node")));
    throw ArgumentUnwrapException();
  }
  xercesc::DOMNode::NodeType type = (xercesc::DOMNode::NodeType)node->getNodeType();
  if ((type == xercesc::DOMNode::COMMENT_NODE) || (type == xercesc::DOMNode::TEXT_NODE) || (type == xercesc::DOMNode::CDATA_SECTION_NODE)) {
    return static_cast<xercesc::DOMCharacterData*>(node);
  }
  else {
    ThrowException(Exception::Error(v8::String::New("incorrect node type")));
    throw ArgumentUnwrapException();
  }
}

xercesc::DOMNodeList *_jsUnwrapNodeList(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_NODE_LIST)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMNodeList")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMNodeList*>(wrap->Value());
}

xercesc::DOMNamedNodeMap *_jsUnwrapNamedNodeMap(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_NAMED_NODE_MAP)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMNamedNodeMap")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMNamedNodeMap*>(wrap->Value());
}

xercesc::DOMTypeInfo *_jsUnwrapTypeInfo(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_TYPE_INFO)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMTypeInfo")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMTypeInfo*>(wrap->Value());
}

#if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
xercesc::DOMStringList *_jsUnwrapDOMStringList(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_STRING_LIST)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMStringList")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMStringList*>(wrap->Value());
}
#endif

xercesc::DOMConfiguration *_jsUnwrapDOMConfiguration(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_CONFIGURATION)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMConfiguration")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMConfiguration*>(wrap->Value());
}

xercesc::DOMError *_jsUnwrapDOMError(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_ERROR)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMError")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMError*>(wrap->Value());
}

xercesc::DOMLocator *_jsUnwrapDOMLocator(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_LOCATION)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMLocator")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMLocator*>(wrap->Value());
}

xercesc::DOMImplementation *_jsUnwrapDOMImplementation(Local<Object> obj) {
  if (!_jsCheckNative(obj, XV8_IMPLEMENTATION)) {
    ThrowException(Exception::Error(v8::String::New("expected DOMImplementation")));
    throw ArgumentUnwrapException();
  }
  Local<External> wrap = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<xercesc::DOMImplementation*>(wrap->Value());
}

xercesc::DOMNode *_jsUnwrapNodeArg(const Arguments& args, int index, xercesc::DOMNode::NodeType type = static_cast<xercesc::DOMNode::NodeType>(0)) {
  if (args.Length() < index + 1) {
    ThrowException(Exception::Error(v8::String::New("not enough arguments")));
    throw ArgumentUnwrapException();
  }
  return _jsUnwrapNode(args[index]->ToObject(), type);
}

bool _jsUnwrapBooleanArg(const Arguments& args, int index) {
  if (args.Length() < index + 1) {
    ThrowException(Exception::Error(v8::String::New("not enough arguments")));
    throw ArgumentUnwrapException();
  }
  return args[index]->ToBoolean()->Value();
}

int _jsUnwrapIntegerArg(const Arguments& args, int index) {
  if (args.Length() < index + 1) {
    ThrowException(Exception::Error(v8::String::New("not enough arguments")));
    throw ArgumentUnwrapException();
  }
  return args[index]->ToInteger()->Value();
}

Handle<Value> PrintCallback(const Arguments& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope;
    if (first) {
      first = false;
    } else {
      std::cout << " ";
    }
    v8::String::Utf8Value str(args[i]);
    std::cout << *str;
  }
  return Undefined();
}

// ================================= DOM API =================================

#if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
Handle<Value> DOMStringList_item(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMStringList *l = _jsUnwrapDOMStringList(args.Holder());
    int index = _jsUnwrapIntegerArg(args, 0);
    return handle_scope.Close<Value>(xv8::String(l->item(index)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
Handle<Value> DOMStringList_length(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMStringList *l = _jsUnwrapDOMStringList(info.Holder());
    return handle_scope.Close(Integer::New(l->getLength()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
Handle<Value> DOMStringList_contains(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMStringList *l = _jsUnwrapDOMStringList(args.Holder());
    xv8::String str(args[0]);
    return handle_scope.Close(Boolean::New(l->contains(str)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NodeList_length(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNodeList *l = _jsUnwrapNodeList(info.Holder());
    return handle_scope.Close<Value>(Integer::New(l->getLength()));
  }
  catch (ArgumentUnwrapException const & ex) {
    return handle_scope.Close(Integer::New(0));
  }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NodeList_item(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNodeList *l = _jsUnwrapNodeList(args.Holder());
    int index = _jsUnwrapIntegerArg(args, 0);
    return handle_scope.Close(_jsCreateNode(l->item(index)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NamedNodeMap_getNamedItem(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateNode(nnm->getNamedItem(name)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NamedNodeMap_setNamedItem(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    xercesc::DOMNode *node = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(_jsCreateNode(nnm->setNamedItem(node)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception calling setNamedItem.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NamedNodeMap_removeNamedItem(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateNode(nnm->removeNamedItem(name)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception calling removeNamedItem.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NamedNodeMap_item(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    int index = _jsUnwrapIntegerArg(args, 0);
    return handle_scope.Close(_jsCreateNode(nnm->item(index)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> NamedNodeMap_length(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(info.Holder());
    return handle_scope.Close(Integer::New(nnm->getLength()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> NamedNodeMap_getNamedItemNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    xv8::String nsuri(args[0]);
    xv8::String localname(args[0]);
    return handle_scope.Close(_jsCreateNode(nnm->getNamedItemNS(nsuri, localname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception calling getNamedItemNS.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> NamedNodeMap_setNamedItemNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    xercesc::DOMNode *node = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(_jsCreateNode(nnm->setNamedItemNS(node)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception calling setNamedItemNS.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> NamedNodeMap_removeNamedItemNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNamedNodeMap *nnm = _jsUnwrapNamedNodeMap(args.Holder());
    xv8::String nsuri(args[0]);
    xv8::String localname(args[0]);
    return handle_scope.Close(_jsCreateNode(nnm->removeNamedItemNS(nsuri, localname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception calling getNamedItemNS.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> DOMImplementation_createDocumentType(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMImplementation *i = _jsUnwrapDOMImplementation(args.Holder());
    xv8::String qname(args[0]);
    xv8::String publicId(args[1]);
    xv8::String systemId(args[2]);
    return handle_scope.Close(_jsCreateNode(i->createDocumentType(qname, publicId, systemId)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception creating DocumentType.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> DOMImplementation_createDocument(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMImplementation *i =_jsUnwrapDOMImplementation(args.Holder());
    xv8::String nsuri(args[0]);
    xv8::String qname(args[1]);
    xercesc::DOMDocumentType *doctype = static_cast<xercesc::DOMDocumentType*>(0);
    if (args[2]->IsObject()) {
      doctype = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(args[2]->ToObject(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    }
    return handle_scope.Close(_jsCreateNode(i->createDocument(nsuri, qname, doctype)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("Exception creating Document.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMCONFIGURATION)
Handle<Value> DOMConfiguration_setParameter(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMConfiguration *c = _jsUnwrapDOMConfiguration(args.Holder());
    xv8::String name(args[0]);
    #if defined (DOM3) && defined (HAVE_DOMCONFIGURATION_BOOLEAN)
    if (_domIsBooleanParam(name)) {
      c->setParameter(name, _jsUnwrapBooleanArg(args, 1));
    }
    else
    #endif
    if (_domIsStringParam(name)) {
      xv8::String value(args[1]);
      c->setParameter(name, (const XMLCh*)value);
    }
    else if (_domIsDOMErrorHandlerParam(name)) {
      c->setParameter(name, new JSDOMErrorHandler(args[1]));
      // TODO: check if it's set, delete previous
    }
    else {
      const Persistent<Value> *value = new Persistent<Value>(args[1]);
      c->setParameter(name, value);
      // TODO: store these in the token based hidden global.
    }
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling DOMConfiguration::setParameter.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMCONFIGURATION)
Handle<Value> DOMConfiguration_getParameter(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMConfiguration *c = _jsUnwrapDOMConfiguration(args.Holder());
    xv8::String name(args[0]);
    const void *ptr = c->getParameter(name);
    #if defined (DOM3) && defined (HAVE_DOMCONFIGURATION_BOOLEAN)
    if (_domIsBooleanParam(name)) {
      return handle_scope.Close(Boolean::New(static_cast<bool>(ptr)));
    }
    else
    #endif
    if (_domIsStringParam(name)) {
      if (ptr) {
        return handle_scope.Close<Value>(xv8::String((const XMLCh*)ptr));
      }
      else {
        return Null();
      }
    }
    else if (_domIsDOMErrorHandlerParam(name)) {
      const JSDOMErrorHandler *errhandler = static_cast<const JSDOMErrorHandler*>(ptr);
      if (errhandler) {
        return handle_scope.Close(errhandler->callback);
      }
      else {
        return Null();
      }
    }
    else {
      ThrowException(Exception::Error(v8::String::New("DOMException: NOT_FOUND_ERR calling DOMConfiguration::getParameter.")));
      return Undefined();
    }
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling DOMConfiguration::getParameter.")));
    return Undefined();
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMCONFIGURATION)
Handle<Value> DOMConfiguration_canSetParameter(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMConfiguration *c = _jsUnwrapDOMConfiguration(args.Holder());
    xv8::String name(args[0]);
    #if defined (DOM3) && defined (HAVE_DOMCONFIGURATION_BOOLEAN)
    if (_domIsBooleanParam(name)) {
      return handle_scope.Close(Boolean::New(c->canSetParameter(name, _jsUnwrapBooleanArg(args, 1))));
    }
    else
    #endif
    if (_domIsStringParam(name)) {
      xv8::String value(args[1]);
      return handle_scope.Close(Boolean::New(c->canSetParameter(name, (const XMLCh*)value)));
    }
    else if (_domIsDOMErrorHandlerParam(name)) {
      // TODO: unwrap dom error handler param
    }
    else {
      return handle_scope.Close(Boolean::New(true));
    }
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling DOMConfiguration::canSetParameter.")));
    return Undefined();
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMCONFIGURATION) && defined (HAVE_DOMSTRINGLIST)
Handle<Value> DOMConfiguration_parameterNames(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMConfiguration *c = _jsUnwrapDOMConfiguration(info.Holder());
    return handle_scope.Close(_jsCreateDOMStringList(c->getParameterNames()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMError_severity(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMError *e = _jsUnwrapDOMError(info.Holder());
    return handle_scope.Close(Integer::New(e->getSeverity()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMError_message(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMError *e = _jsUnwrapDOMError(info.Holder());
    return handle_scope.Close<Value>(xv8::String(e->getMessage()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMError_type(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMError *e = _jsUnwrapDOMError(info.Holder());
    return handle_scope.Close<Value>(xv8::String(e->getType()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMError_location(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMError *e = _jsUnwrapDOMError(info.Holder());
    return handle_scope.Close(_jsCreateDOMLocator(e->getLocation()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMLocator_lineNumber(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMLocator *l = _jsUnwrapDOMLocator(info.Holder());
    return handle_scope.Close(Integer::New(l->getLineNumber()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMLocator_columnNumber(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMLocator *l = _jsUnwrapDOMLocator(info.Holder());
    return handle_scope.Close(Integer::New(l->getColumnNumber()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMLOCATOR_GETBYTEOFFSET)
Handle<Value> DOMLocator_byteOffset(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMLocator *l = _jsUnwrapDOMLocator(info.Holder());
    return handle_scope.Close(Integer::New(l->getByteOffset()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMLOCATOR_GETUTF16OFFSET)
Handle<Value> DOMLocator_utf16Offset(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMLocator *l = _jsUnwrapDOMLocator(info.Holder());
    return handle_scope.Close(Integer::New(l->getUtf16Offset()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMLOCATOR_GETRELATEDNODE)
Handle<Value> DOMLocator_relatedNode(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMLocator *l = _jsUnwrapDOMLocator(info.Holder());
    return handle_scope.Close(_jsCreateNode(l->getRelatedNode()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> DOMLocator_uri(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMLocator *l = _jsUnwrapDOMLocator(info.Holder());
    return handle_scope.Close<Value>(xv8::String(l->getURI()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_nodeName(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getNodeName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_nodeValue(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getNodeValue()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException retrieving nodeValue.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
void Node_setNodeValue(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    node->setNodeValue(xv8::String(node->getNodeValue()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException: NO_MODIFICATION_ALLOWED_ERR while calling DOMNode::setNodeValue.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM1)
Handle<Value> Node_nodeType(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(Integer::New(node->getNodeType()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_parentNode(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateNode(node->getParentNode()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_childNodes(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateDOMNodeList(node->getChildNodes()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_firstChild(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateNode(node->getFirstChild()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_lastChild(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateNode(node->getLastChild()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_previousSibling(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateNode(node->getPreviousSibling()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_nextSibling(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateNode(node->getNextSibling()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_ownerDocument(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close(_jsCreateNode(node->getOwnerDocument()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_insertBefore(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *newChild = _jsUnwrapNodeArg(args, 0);
    xercesc::DOMNode *refChild = _jsUnwrapNodeArg(args, 1);
    return handle_scope.Close(_jsCreateNode(node->insertBefore(newChild, refChild)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling insertBefore.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_replaceChild(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *newChild = _jsUnwrapNodeArg(args, 0);
    xercesc::DOMNode *oldChild = _jsUnwrapNodeArg(args, 1);
    return handle_scope.Close(_jsCreateNode(node->insertBefore(newChild, oldChild)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling replaceChild.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_removeChild(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *oldChild = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(_jsCreateNode(node->removeChild(oldChild)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling removeChild.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_appendChild(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *newChild = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(_jsCreateNode(node->appendChild(newChild)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling appendChild.")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_hasChildNodes(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    return handle_scope.Close(Boolean::New(node->hasChildNodes()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_cloneNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    bool deep = _jsUnwrapBooleanArg(args, 0);
    return handle_scope.Close(_jsCreateNode(node->cloneNode(deep)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Node_normalize(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    node->normalize();
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Node_isSupported(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xv8::String feature(args[0]);
    xv8::String version(args[1]);
    return handle_scope.Close(Boolean::New(node->isSupported(feature, version)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Node_namespaceURI(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getNamespaceURI()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Node_prefix(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getPrefix()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
void Node_setPrefix(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    xv8::String v(value->ToString());
    node->setPrefix(v);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException setting prefix")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM2)
Handle<Value> Node_localName(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getLocalName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Node_hasAttributes(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    return handle_scope.Close(Boolean::New(node->hasAttributes()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Node_baseURI(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getBaseURI()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMNODE_COMPAREDOCUMENTPOSITION)
Handle<Value> Node_compareDocumentPosition(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *other = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(Integer::New(node->compareDocumentPosition(other)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling compareDocumentPosition")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Node_textContent(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(node->getTextContent()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException getting textContent")));
  }
  return Undefined();
}
#endif

#if defined (DOM3)
void Node_setTextContent(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(info.Holder());
    node->setTextContent(xv8::String(value->ToString()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException getting textContent")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM3)
Handle<Value> Node_isSameNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *other = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(Boolean::New(node->isSameNode(other)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMNODE_LOOKUPPREFIX)
Handle<Value> Node_lookupPrefix(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xv8::String nsuri(args[0]);
    return handle_scope.Close<Value>(xv8::String(node->lookupPrefix(nsuri)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Node_isDefaultNamespace(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xv8::String nsuri(args[0]);
    return handle_scope.Close(Boolean::New(node->isDefaultNamespace(nsuri)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Node_lookupNamespaceURI(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xv8::String prefix(args[0]);
    return handle_scope.Close<Value>(xv8::String(node->lookupNamespaceURI(prefix)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Node_isEqualNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    xercesc::DOMNode *other =_jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(Boolean::New(node->isEqualNode(other)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif


Handle<Object> nodeUserDataObject(xercesc::DOMNode *n) {
  return nodeObject(n, "DOMUserData");
}

Handle<Object> nodeErrorHandlerObject(xercesc::DOMNode *n) {
  return nodeObject(n, "DOMErrorHandler");
}

#if defined (DOM3)
Handle<Value> Node_setUserData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    Handle<Object> userdata = nodeUserDataObject(node);
    userdata->Set(args[0], args[1]);
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return handle_scope.Close(args[1]);
}
#endif

#if defined (DOM3)
Handle<Value> Node_getUserData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNode *node = _jsUnwrapNode(args.Holder());
    Handle<Object> userdata = nodeUserDataObject(node);
    return userdata->Get(args[0]);
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> GetElementAttributes(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    return handle_scope.Close(_jsCreateDOMNamedNodeMap(e->getAttributes()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_tagName(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getTagName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_getAttribute(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close<Value>(xv8::String(e->getAttribute(name)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_setAttribute(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    xv8::String value(args[1]);
    e->setAttribute(name, value);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling setAttribute")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_removeAttribute(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    e->removeAttribute(name);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling removeAttribute")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_getAttributeNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateNode(e->getAttributeNode(name)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_setAttributeNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xercesc::DOMAttr *attr =  static_cast<xercesc::DOMAttr*>(_jsUnwrapNodeArg(args, 0, xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(_jsCreateNode(e->setAttributeNode(attr)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling setAttributeNode")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_removeAttributeNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xercesc::DOMAttr *attr =  static_cast<xercesc::DOMAttr*>(_jsUnwrapNodeArg(args, 0, xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(_jsCreateNode(e->removeAttributeNode(attr)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling setAttributeNode")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Element_getElementsByTagName(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateDOMNodeList(e->getElementsByTagName(name)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_getAttributeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    return handle_scope.Close<Value>(xv8::String(e->getAttributeNS(nsuri, localname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::getAttributeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_setAttributeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    xv8::String value(args[2]);
    e->setAttributeNS(nsuri, localname, value);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::setAttributeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_removeAttributeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    e->removeAttributeNS(nsuri, localname);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::removeAttributeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_getAttributeNodeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    return handle_scope.Close(_jsCreateNode(e->getAttributeNodeNS(nsuri, localname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::getAttributeNodeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_setAttributeNodeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xercesc::DOMAttr *attr =  static_cast<xercesc::DOMAttr*>(_jsUnwrapNodeArg(args, 0, xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(_jsCreateNode(e->setAttributeNodeNS(attr)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::setAttributeNodeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_getElementsByTagNameNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    return handle_scope.Close(_jsCreateDOMNodeList(e->getElementsByTagNameNS(nsuri, localname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::getElementsByTagNameNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_hasAttribute(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(Boolean::New(e->hasAttribute(name)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Element_hasAttributeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    return handle_scope.Close(Boolean::New(e->hasAttributeNS(nsuri, localname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::hasAttributeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMELEMENT_GETSCHEMATYPEINFO)
Handle<Value> Element_schemaTypeInfo(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    return handle_scope.Close(_jsCreateDOMTypeInfo(e->getSchemaTypeInfo()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Element_setIdAttribute(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String name(args[0]);
    #if defined (HAVE_DOMELEMENT_SETIDATTRIBUTE_2ARG)
      bool isid = true;
      if (!args[1]->IsUndefined()) {
        isid = _jsUnwrapBooleanArg(args, 1);
      }
      e->setIdAttribute(name, isid);
    #else
      // TODO: if 2 arg version is given, warn?
      e->setIdAttribute(name);
    #endif
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::setIdAttribute")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Element_setIdAttributeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    #if defined (HAVE_DOMELEMENT_SETIDATTRIBUTENS_3ARG)
      bool isid = true;
      if (!args[2]->IsUndefined()) {
        isid = _jsUnwrapBooleanArg(args, 2);
      }
      e->setIdAttributeNS(nsuri, localname, isid);
    #else
      // TODO: if 3 arg version is given, warn?
      e->setIdAttributeNS(nsuri, localname);
    #endif
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::setIdAttributeNS")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Element_setIdAttributeNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::ELEMENT_NODE));
    xercesc::DOMAttr *attr = static_cast<xercesc::DOMAttr*>(_jsUnwrapNodeArg(args, 0, xercesc::DOMNode::ATTRIBUTE_NODE));
    #if defined (HAVE_DOMELEMENT_SETIDATTRIBUTENODE_2ARG)
      bool isid = true;
      if (!(args[1]->IsUndefined())) {
        isid = _jsUnwrapBooleanArg(args, 1);
      }
      e->setIdAttributeNode(attr, isid);
    #else
      // TODO: if 2 arg version is given, warn?
      e->setIdAttributeNode(attr);
    #endif
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("DOMException calling Element::setIdAttributeNode")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_data(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(info.Holder());
    return handle_scope.Close<Value>(xv8::String(d->getData()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception fetching character data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
void CharacterData_setData(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(info.Holder());
    d->setData(xv8::String(value->ToString()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception setting character data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_length(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(info.Holder());
    return handle_scope.Close(Integer::New(d->getLength()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_substringData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(args.Holder());
    int offset = _jsUnwrapIntegerArg(args, 0);
    int count = _jsUnwrapIntegerArg(args, 1);
    return handle_scope.Close<Value>(xv8::String(d->substringData(offset, count)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception fetching substring data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_appendData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(args.Holder());
    xv8::String arg(args[0]);
    d->appendData(arg);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception appending data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_insertData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(args.Holder());
    int offset = _jsUnwrapIntegerArg(args, 0);
    xv8::String arg(args[1]);
    d->insertData(offset, arg);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception inserting data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_deleteData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(args.Holder());
    int offset = _jsUnwrapIntegerArg(args, 0);
    int count = _jsUnwrapIntegerArg(args, 1);
    d->deleteData(offset, count);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception deleting data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> CharacterData_replaceData(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMCharacterData *d = _jsUnwrapCharacterDataNode(args.Holder());
    int offset = _jsUnwrapIntegerArg(args, 0);
    int count = _jsUnwrapIntegerArg(args, 1);
    xv8::String arg(args[2]);
    d->replaceData(offset, count, arg);
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception replacing data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Attr_name(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close<Value>(xv8::String(a->getName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Attr_specified(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(Boolean::New(a->getSpecified()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Attr_value(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close<Value>(xv8::String(a->getValue()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
void Attr_setValue(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    a->setValue(xv8::String(value->ToString()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception setting value")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM2)
Handle<Value> Attr_ownerElement(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(_jsCreateNode(a->getOwnerElement()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMATTR_GETSCHEMATYPEINFO)
Handle<Value> Attr_schemaTypeInfo(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(_jsCreateDOMTypeInfo(a->getSchemaTypeInfo()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Attr_isId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMAttr *a = static_cast<xercesc::DOMAttr*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ATTRIBUTE_NODE));
    return handle_scope.Close(Boolean::New(a->isId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_doctype(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(_jsCreateNode(doc->getDoctype()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_implementation(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(_jsCreateDOMImplementation(doc->getImplementation()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_documentElement(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(_jsCreateNode(doc->getDocumentElement()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createElement(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->createElement(name)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating Element")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createDocumentFragment(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(_jsCreateNode(doc->createDocumentFragment()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createTextNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String data(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->createTextNode(data)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createComment(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String data(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->createComment(data)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createCDATASection(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String data(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->createCDATASection(data)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating CDATASection")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createProcessingInstruction(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String target(args[0]);
    xv8::String data(args[1]);
    return handle_scope.Close(_jsCreateNode(doc->createProcessingInstruction(target, data)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating ProcessingInstruction")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createAttribute(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->createAttribute(name)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating Attribute")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_createEntityReference(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->createEntityReference(name)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating EntityReference")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Document_getElementsByTagName(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String name(args[0]);
    return handle_scope.Close(_jsCreateDOMNodeList(doc->getElementsByTagName(name)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Document_importNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xercesc::DOMNode *node = _jsUnwrapNodeArg(args, 0);
    bool deep = _jsUnwrapBooleanArg(args, 1);
    return handle_scope.Close(_jsCreateNode(doc->importNode(node, deep)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception importing node")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Document_createElementNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String qname(args[1]);
    return handle_scope.Close(_jsCreateNode(doc->createElementNS(nsuri, qname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating Element with namespace")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Document_createAttributeNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String qname(args[1]);
    return handle_scope.Close(_jsCreateNode(doc->createAttributeNS(nsuri, qname)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating Attribute with namespace")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Document_getElementsByTagNameNS(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String nsuri(args[0]);
    xv8::String localname(args[1]);
    return handle_scope.Close(_jsCreateDOMNodeList(doc->getElementsByTagNameNS(nsuri, localname)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> Document_getElementById(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xv8::String id(args[0]);
    return handle_scope.Close(_jsCreateNode(doc->getElementById(id)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception creating EntityReference")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_GETINPUTENCODING)
Handle<Value> Document_inputEncoding(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close<Value>(xv8::String(doc->getInputEncoding()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_GETXMLENCODING)
Handle<Value> Document_xmlEncoding(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close<Value>(xv8::String(doc->getXmlEncoding()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_GETXMLSTANDALONE)
Handle<Value> Document_xmlStandalone(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(Boolean::New(doc->getXmlStandalone()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_SETXMLSTANDALONE)
void Document_setXmlStandalone(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    doc->setXmlStandalone(property->ToBoolean()->Value());
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception setting xmlStandalone")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_GETXMLVERSION)
Handle<Value> Document_xmlVersion(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close<Value>(xv8::String(doc->getXmlVersion()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_SETXMLVERSION)
void Document_setXmlVersion(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    doc->setXmlVersion(xv8::String(value->ToString()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception setting xmlVersion")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM3)
Handle<Value> Document_strictErrorChecking(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(Boolean::New(doc->getStrictErrorChecking()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
void Document_setStrictErrorChecking(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    doc->setStrictErrorChecking(property->ToBoolean()->Value());
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM3)
Handle<Value> Document_documentURI(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close<Value>(xv8::String(doc->getDocumentURI()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
void Document_setDocumentURI(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    doc->setDocumentURI(xv8::String(value->ToString()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM3)
Handle<Value> Document_adoptNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xercesc::DOMNode *node = _jsUnwrapNodeArg(args, 0);
    return handle_scope.Close(_jsCreateNode(doc->adoptNode(node)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception adopting node")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMDOCUMENT_GETDOMCONFIG)
Handle<Value> Document_domConfig(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    return handle_scope.Close(_jsCreateDOMConfiguration(doc->getDOMConfig()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Document_normalizeDocument(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    doc->normalizeDocument();
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3)
Handle<Value> Document_renameNode(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocument *doc = static_cast<xercesc::DOMDocument*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::DOCUMENT_NODE));
    xercesc::DOMNode *node = _jsUnwrapNodeArg(args, 0);
    xv8::String nsuri(args[1]);
    xv8::String qname(args[2]);
    return handle_scope.Close(_jsCreateNode(doc->renameNode(node, nsuri, qname)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> DocumentType_name(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocumentType *t = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    return handle_scope.Close<Value>(xv8::String(t->getName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> DocumentType_entities(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocumentType *t = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    return handle_scope.Close(_jsCreateDOMNamedNodeMap(t->getEntities()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> DocumentType_notations(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocumentType *t = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    return handle_scope.Close(_jsCreateDOMNamedNodeMap(t->getNotations()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> DocumentType_publicId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocumentType *t = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    return handle_scope.Close<Value>(xv8::String(t->getPublicId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> DocumentType_systemId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocumentType *t = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    return handle_scope.Close<Value>(xv8::String(t->getSystemId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM2)
Handle<Value> DocumentType_internalSubset(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMDocumentType *t = static_cast<xercesc::DOMDocumentType*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::DOCUMENT_TYPE_NODE));
    return handle_scope.Close<Value>(xv8::String(t->getInternalSubset()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Entity_publicId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMEntity *e = static_cast<xercesc::DOMEntity*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ENTITY_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getPublicId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Entity_systemId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMEntity *e = static_cast<xercesc::DOMEntity*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ENTITY_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getSystemId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Entity_notationName(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMEntity *e = static_cast<xercesc::DOMEntity*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ENTITY_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getNotationName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMENTITY_GETINPUTENCODING)
Handle<Value> Entity_inputEncoding(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMEntity *e = static_cast<xercesc::DOMEntity*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ENTITY_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getInputEncoding()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMENTITY_GETXMLENCODING)
Handle<Value> Entity_xmlEncoding(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMEntity *e = static_cast<xercesc::DOMEntity*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ENTITY_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getXmlEncoding()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMENTITY_GETXMLVERSION)
Handle<Value> Entity_xmlVersion(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMEntity *e = static_cast<xercesc::DOMEntity*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::ENTITY_NODE));
    return handle_scope.Close<Value>(xv8::String(e->getXmlVersion()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Notation_publicId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNotation *n = static_cast<xercesc::DOMNotation*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::NOTATION_NODE));
    return handle_scope.Close<Value>(xv8::String(n->getPublicId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> Notation_systemId(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMNotation *n = static_cast<xercesc::DOMNotation*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::NOTATION_NODE));
    return handle_scope.Close<Value>(xv8::String(n->getSystemId()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> ProcessingInstruction_target(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMProcessingInstruction *pi = static_cast<xercesc::DOMProcessingInstruction*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::PROCESSING_INSTRUCTION_NODE));
    return handle_scope.Close<Value>(xv8::String(pi->getTarget()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
Handle<Value> ProcessingInstruction_data(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMProcessingInstruction *pi = static_cast<xercesc::DOMProcessingInstruction*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::PROCESSING_INSTRUCTION_NODE));
    return handle_scope.Close<Value>(xv8::String(pi->getData()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM1)
void ProcessingInstruction_setData(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMProcessingInstruction *pi = static_cast<xercesc::DOMProcessingInstruction*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::PROCESSING_INSTRUCTION_NODE));
    pi->setData(xv8::String(value->ToString()));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception setting ProcessingInstruction data")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
}
#endif

#if defined (DOM1)
Handle<Value> Text_splitText(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMText *t = static_cast<xercesc::DOMText*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::TEXT_NODE));
    int offset = _jsUnwrapIntegerArg(args, 0);
    return handle_scope.Close(_jsCreateNode(t->splitText(offset)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception in splitText")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined(DOM3) && defined(HAVE_DOMTEXT_GETISELEMENTCONTENTWHITESPACE)
Handle<Value> Text_isElementContentWhitespace(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMText *t = static_cast<xercesc::DOMText*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::TEXT_NODE));
    return handle_scope.Close(Boolean::New(t->getIsElementContentWhitespace()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined(DOM3)
Handle<Value> Text_wholeText(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMText *t = static_cast<xercesc::DOMText*>(_jsUnwrapNode(info.Holder(), xercesc::DOMNode::TEXT_NODE));
    return handle_scope.Close<Value>(xv8::String(t->getWholeText()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined(DOM3)
Handle<Value> Text_replaceWholeText(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMText *t = static_cast<xercesc::DOMText*>(_jsUnwrapNode(args.Holder(), xercesc::DOMNode::TEXT_NODE));
    xv8::String content(args[0]);
    return handle_scope.Close(_jsCreateNode(t->replaceWholeText(content)));
  }
  catch (xercesc::DOMException const & ex) {
    ThrowException(Exception::Error(v8::String::New("exception in replaceWholeText")));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMTYPEINFO_GETTYPENAME)
Handle<Value> TypeInfo_typeName(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMTypeInfo *ti = _jsUnwrapTypeInfo(info.Holder());
    return handle_scope.Close<Value>(xv8::String(ti->getTypeName()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMTYPEINFO_GETTYPENAMESPACE)
Handle<Value> TypeInfo_typeNamespace(Local<v8::String> property, const AccessorInfo &info) {
  HandleScope handle_scope;
  try {
    xercesc::DOMTypeInfo *ti = _jsUnwrapTypeInfo(info.Holder());
    return handle_scope.Close<Value>(xv8::String(ti->getTypeNamespace()));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

#if defined (DOM3) && defined (HAVE_DOMTYPEINFO_ISDERIVEDFROM)
Handle<Value> TypeInfo_isDerivedFrom(const Arguments& args) {
  HandleScope handle_scope;
  try {
    xercesc::DOMTypeInfo *ti = _jsUnwrapTypeInfo(args.Holder());
    xv8::String ns(args[0]);
    xv8::String name(args[1]);
    xercesc::DOMTypeInfo::DerivationMethods method = static_cast<xercesc::DOMTypeInfo::DerivationMethods>(_jsUnwrapIntegerArg(args, 2));
    return handle_scope.Close(Boolean::New(ti->isDerivedFrom(ns, name, method)));
  }
  catch (ArgumentUnwrapException const & ex) { ; }
  return Undefined();
}
#endif

/**
 *  v8 "getter" callback that always returns NULL. Provides a quick exit for
 *  prototype functions that are readonly and defined in DOM level 3 as NULL.
 */
Handle<Value> GetNull(Local<v8::String> property, const AccessorInfo &info) {
  return Null();
}

/**
 *  v8 function that always returns false. Provides a quick exit for
 *  prototype functions that are not yet implemented.
 */
Handle<Value> FuncGetFalse(const Arguments& args) {
  return Boolean::New(false);
}

/**
 *  v8 "setter" callback that always throws a NO_MODIFICATION_ALLOWED_ERR.
 *  Provides a quick exit for prototype functions that must raise an exception
 *  upon setting according to spec.
 */
void SetModificationErr(Local<v8::String> property, Local<Value> value, const AccessorInfo& info) {
  ThrowException(Exception::Error(v8::String::New("DOMException: NO_MODIFICATION_ALLOWED_ERR.")));
}

class Templates {
  public:
    Handle<ObjectTemplate> global;
    Handle<FunctionTemplate> namednodemap, nodelist, typeinfo, implementation, configuration, stringlist, error, locator;
    Handle<FunctionTemplate> node, characterdata, attr, cdatasection, comment, document, documentfragment, documenttype, element, entity, entityreference, notation, processinginstruction, text;
    Templates() :
      global(ObjectTemplate::New()),
      namednodemap(FunctionTemplate::New()),
      nodelist(FunctionTemplate::New()),
      typeinfo(FunctionTemplate::New()),
      implementation(FunctionTemplate::New()),
      configuration(FunctionTemplate::New()),
      stringlist(FunctionTemplate::New()),
      error(FunctionTemplate::New()),
      locator(FunctionTemplate::New()),
      node(FunctionTemplate::New()),
      characterdata(FunctionTemplate::New()),
      attr(FunctionTemplate::New()),
      cdatasection(FunctionTemplate::New()),
      comment(FunctionTemplate::New()),
      document(FunctionTemplate::New()),
      documentfragment(FunctionTemplate::New()),
      documenttype(FunctionTemplate::New()),
      element(FunctionTemplate::New()),
      entity(FunctionTemplate::New()),
      entityreference(FunctionTemplate::New()),
      notation(FunctionTemplate::New()),
      processinginstruction(FunctionTemplate::New()),
      text(FunctionTemplate::New())
    {
      // Global
      global->Set(v8::String::New("print"), FunctionTemplate::New(PrintCallback), ReadOnly);

      // NamedNodeMap
      namednodemap->SetClassName(v8::String::New("NamedNodeMap"));
      Local<ObjectTemplate> prototype = namednodemap->PrototypeTemplate();
      Local<ObjectTemplate> instance = namednodemap->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set(v8::String::New("getNamedItem"), FunctionTemplate::New(NamedNodeMap_getNamedItem), ReadOnly);
      prototype->Set(v8::String::New("setNamedItem"), FunctionTemplate::New(NamedNodeMap_setNamedItem), ReadOnly);
      prototype->Set(v8::String::New("removeNamedItem"), FunctionTemplate::New(NamedNodeMap_removeNamedItem), ReadOnly);
      prototype->Set(v8::String::New("item"), FunctionTemplate::New(NamedNodeMap_item), ReadOnly);
      instance->SetAccessor(v8::String::New("length"), NamedNodeMap_length);
      prototype->Set(v8::String::New("getNamedItemNS"), FunctionTemplate::New(NamedNodeMap_getNamedItemNS), ReadOnly);
      prototype->Set(v8::String::New("setNamedItemNS"), FunctionTemplate::New(NamedNodeMap_setNamedItemNS), ReadOnly);
      prototype->Set(v8::String::New("removeNamedItemNS"), FunctionTemplate::New(NamedNodeMap_removeNamedItemNS), ReadOnly);

      // NodeList
      nodelist->SetClassName(v8::String::New("NodeList"));
      prototype = nodelist->PrototypeTemplate();
      instance = nodelist->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("length"), NodeList_length);
      prototype->Set(v8::String::New("item"), FunctionTemplate::New(NodeList_item), ReadOnly);

      // TypeInfo
      typeinfo->SetClassName(v8::String::New("TypeInfo"));
      prototype = typeinfo->PrototypeTemplate();
      instance = typeinfo->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      #if defined (HAVE_DOMTYPEINFO_GETTYPENAME)
        instance->SetAccessor(v8::String::New("typeName"), TypeInfo_typeName);
      #endif
      #if defined (HAVE_DOMTYPEINFO_GETTYPENAMESPACE)
        instance->SetAccessor(v8::String::New("typeNamespace"), TypeInfo_typeNamespace);
      #endif
      #if defined (HAVE_DOMTYPEINFO_ISDERIVEDFROM)
        prototype->Set("DERIVATION_RESTRICTION", v8::Number::New(0x00000001));
        prototype->Set("DERIVATION_EXTENSION", v8::Number::New(0x00000002));
        prototype->Set("DERIVATION_UNION", v8::Number::New(0x00000004));
        prototype->Set("DERIVATION_LIST", v8::Number::New(0x00000008));
        prototype->Set(v8::String::New("isDerivedFrom"), FunctionTemplate::New(TypeInfo_isDerivedFrom), ReadOnly);
      #endif

      // DOMImplementation
      implementation->SetClassName(v8::String::New("DOMImplementation"));
      prototype = implementation->PrototypeTemplate();
      instance = implementation->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set(v8::String::New("hasFeature"), FunctionTemplate::New(FuncGetFalse), ReadOnly);
      prototype->Set(v8::String::New("createDocumentType"), FunctionTemplate::New(DOMImplementation_createDocumentType), ReadOnly);
      prototype->Set(v8::String::New("createDocument"), FunctionTemplate::New(DOMImplementation_createDocument), ReadOnly);
      prototype->Set(v8::String::New("getFeature"), FunctionTemplate::New(FuncGetFalse), ReadOnly);

      // DOMConfiguration
      configuration->SetClassName(v8::String::New("DOMConfiguration"));
      prototype = configuration->PrototypeTemplate();
      instance = configuration->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set(v8::String::New("setParameter"), FunctionTemplate::New(DOMConfiguration_setParameter), ReadOnly);
      prototype->Set(v8::String::New("getParameter"), FunctionTemplate::New(DOMConfiguration_getParameter), ReadOnly);
      prototype->Set(v8::String::New("canSetParameter"), FunctionTemplate::New(DOMConfiguration_canSetParameter), ReadOnly);
      #if defined (DOM3) && defined (HAVE_DOMCONFIGURATION) && defined (HAVE_DOMSTRINGLIST)
        instance->SetAccessor(v8::String::New("parameterNames"), DOMConfiguration_parameterNames);
      #endif

      // DOMStringList
      #if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
      stringlist->SetClassName(v8::String::New("DOMStringList"));
      prototype = stringlist->PrototypeTemplate();
      instance = stringlist->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set(v8::String::New("item"), FunctionTemplate::New(DOMStringList_item), ReadOnly);
      instance->SetAccessor(v8::String::New("length"), DOMStringList_length);
      prototype->Set(v8::String::New("contains"), FunctionTemplate::New(DOMStringList_contains), ReadOnly);
      #endif

      // DOMError
      error->SetClassName(v8::String::New("DOMError"));
      prototype = error->PrototypeTemplate();
      instance = error->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set("SEVERITY_WARNING", v8::Number::New(1));
      prototype->Set("SEVERITY_ERROR", v8::Number::New(2));
      prototype->Set("SEVERITY_FATAL_ERROR", v8::Number::New(3));
      instance->SetAccessor(v8::String::New("severity"), DOMError_severity);
      instance->SetAccessor(v8::String::New("message"), DOMError_message);
      instance->SetAccessor(v8::String::New("type"), DOMError_type);
      instance->SetAccessor(v8::String::New("relatedException"), GetNull);
      instance->SetAccessor(v8::String::New("relatedData"), GetNull);
      instance->SetAccessor(v8::String::New("location"), DOMError_location);

      // DOMLocator
      #if defined (DOM3)
        locator->SetClassName(v8::String::New("DOMLocator"));
        prototype = locator->PrototypeTemplate();
        instance = locator->InstanceTemplate();
        instance->SetInternalFieldCount(2);
        instance->SetAccessor(v8::String::New("lineNumber"), DOMLocator_lineNumber);
        instance->SetAccessor(v8::String::New("columnNumber"), DOMLocator_columnNumber);
        #if defined (HAVE_DOMLOCATOR_GETBYTEOFFSET)
          instance->SetAccessor(v8::String::New("byteOffset"), DOMLocator_byteOffset);
        #endif
        #if defined (HAVE_DOMLOCATOR_GETUTF16OFFSET)
          instance->SetAccessor(v8::String::New("utf16Offset"), DOMLocator_utf16Offset);
        #endif
        #if defined (HAVE_DOMLOCATOR_GETRELATEDNODE)
          instance->SetAccessor(v8::String::New("relatedNode"), DOMLocator_relatedNode);
        #endif
        instance->SetAccessor(v8::String::New("uri"), DOMLocator_uri);
      #endif

      // Node
      node->SetClassName(v8::String::New("Node"));
      prototype = node->PrototypeTemplate();
      instance = node->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set("ELEMENT_NODE", v8::Number::New(1));
      prototype->Set("ATTRIBUTE_NODE", v8::Number::New(2));
      prototype->Set("TEXT_NODE", v8::Number::New(3));
      prototype->Set("CDATA_SECTION_NODE", v8::Number::New(4));
      prototype->Set("ENTITY_REFERENCE_NODE", v8::Number::New(5));
      prototype->Set("ENTITY_NODE", v8::Number::New(6));
      prototype->Set("PROCESSING_INSTRUCTION_NODE", v8::Number::New(7));
      prototype->Set("COMMENT_NODE", v8::Number::New(8));
      prototype->Set("DOCUMENT_NODE", v8::Number::New(9));
      prototype->Set("DOCUMENT_TYPE_NODE", v8::Number::New(10));
      prototype->Set("DOCUMENT_FRAGMENT_NODE", v8::Number::New(11));
      prototype->Set("NOTATION_NODE", v8::Number::New(12));
      instance->SetAccessor(v8::String::New("nodeName"), Node_nodeName);
      instance->SetAccessor(v8::String::New("nodeValue"), Node_nodeValue, Node_setNodeValue);
      instance->SetAccessor(v8::String::New("nodeType"), Node_nodeType);
      instance->SetAccessor(v8::String::New("parentNode"), Node_parentNode);
      instance->SetAccessor(v8::String::New("childNodes"), Node_childNodes);
      instance->SetAccessor(v8::String::New("firstChild"), Node_firstChild);
      instance->SetAccessor(v8::String::New("lastChild"), Node_lastChild);
      instance->SetAccessor(v8::String::New("previousSibling"), Node_previousSibling);
      instance->SetAccessor(v8::String::New("nextSibling"), Node_nextSibling);
      instance->SetAccessor(v8::String::New("attributes"), GetNull); // Overridden in Element.
      instance->SetAccessor(v8::String::New("ownerDocument"), Node_ownerDocument);
      prototype->Set(v8::String::New("insertBefore"), FunctionTemplate::New(Node_insertBefore), ReadOnly);
      prototype->Set(v8::String::New("replaceChild"), FunctionTemplate::New(Node_replaceChild), ReadOnly);
      prototype->Set(v8::String::New("removeChild"), FunctionTemplate::New(Node_removeChild), ReadOnly);
      prototype->Set(v8::String::New("appendChild"), FunctionTemplate::New(Node_appendChild), ReadOnly);
      prototype->Set(v8::String::New("hasChildNodes"), FunctionTemplate::New(Node_hasChildNodes), ReadOnly);
      prototype->Set(v8::String::New("cloneNode"), FunctionTemplate::New(Node_cloneNode), ReadOnly);
      prototype->Set(v8::String::New("normalize"), FunctionTemplate::New(Node_normalize), ReadOnly);
      prototype->Set(v8::String::New("isSupported"), FunctionTemplate::New(Node_isSupported), ReadOnly);
      instance->SetAccessor(v8::String::New("namespaceURI"), Node_namespaceURI);
      instance->SetAccessor(v8::String::New("prefix"), Node_prefix, Node_setPrefix);
      instance->SetAccessor(v8::String::New("localName"), Node_localName);
      prototype->Set(v8::String::New("hasAttributes"), FunctionTemplate::New(Node_hasAttributes), ReadOnly);
      instance->SetAccessor(v8::String::New("baseURI"), Node_baseURI);
      prototype->Set("DOCUMENT_POSITION_DISCONNECTED", v8::Number::New(0x01));
      prototype->Set("DOCUMENT_POSITION_PRECEDING", v8::Number::New(0x02));
      prototype->Set("DOCUMENT_POSITION_FOLLOWING", v8::Number::New(0x04));
      prototype->Set("DOCUMENT_POSITION_CONTAINS", v8::Number::New(0x08));
      prototype->Set("DOCUMENT_POSITION_CONTAINED_BY", v8::Number::New(0x10));
      prototype->Set("DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", v8::Number::New(0x20));
      #if defined(HAVE_DOMNODE_COMPAREDOCUMENTPOSITION)
        prototype->Set(v8::String::New("compareDocumentPosition"), FunctionTemplate::New(Node_compareDocumentPosition), ReadOnly);
      #endif
      instance->SetAccessor(v8::String::New("textContent"), Node_textContent, Node_setTextContent);
      prototype->Set(v8::String::New("isSameNode"), FunctionTemplate::New(Node_isSameNode), ReadOnly);
      #if defined(HAVE_DOMNODE_LOOKUPPREFIX)
        prototype->Set(v8::String::New("lookupPrefix"), FunctionTemplate::New(Node_lookupPrefix), ReadOnly);
      #endif
      prototype->Set(v8::String::New("isDefaultNamespace"), FunctionTemplate::New(Node_isDefaultNamespace), ReadOnly);
      prototype->Set(v8::String::New("lookupNamespaceURI"), FunctionTemplate::New(Node_lookupNamespaceURI), ReadOnly);
      prototype->Set(v8::String::New("isEqualNode"), FunctionTemplate::New(Node_isEqualNode), ReadOnly);
      prototype->Set(v8::String::New("getFeature"), FunctionTemplate::New(FuncGetFalse), ReadOnly);
      prototype->Set(v8::String::New("setUserData"), FunctionTemplate::New(Node_setUserData), ReadOnly);
      prototype->Set(v8::String::New("getUserData"), FunctionTemplate::New(Node_getUserData), ReadOnly);

      // CharacterData
      characterdata->Inherit(node);
      characterdata->SetClassName(v8::String::New("CharacterData"));
      prototype = characterdata->PrototypeTemplate();
      instance = characterdata->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("textContent"), CharacterData_data, CharacterData_setData);
      instance->SetAccessor(v8::String::New("length"), CharacterData_length);
      prototype->Set(v8::String::New("substringData"), FunctionTemplate::New(CharacterData_substringData), ReadOnly);
      prototype->Set(v8::String::New("appendData"), FunctionTemplate::New(CharacterData_appendData), ReadOnly);
      prototype->Set(v8::String::New("insertData"), FunctionTemplate::New(CharacterData_insertData), ReadOnly);
      prototype->Set(v8::String::New("deleteData"), FunctionTemplate::New(CharacterData_deleteData), ReadOnly);
      prototype->Set(v8::String::New("replaceData"), FunctionTemplate::New(CharacterData_replaceData), ReadOnly);

      // Attr
      attr->Inherit(node);
      attr->SetClassName(v8::String::New("Attr"));
      prototype = attr->PrototypeTemplate();
      instance = attr->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("name"), Attr_name);
      instance->SetAccessor(v8::String::New("specified"), Attr_specified);
      instance->SetAccessor(v8::String::New("value"), Attr_value, Attr_setValue);
      instance->SetAccessor(v8::String::New("ownerElement"), Attr_ownerElement);
      #if defined(HAVE_DOMATTR_GETSCHEMATYPEINFO)
        instance->SetAccessor(v8::String::New("schemaTypeInfo"), Attr_schemaTypeInfo);
      #endif
      instance->SetAccessor(v8::String::New("isId"), Attr_isId);

      // Comment
      comment->Inherit(characterdata);
      comment->SetClassName(v8::String::New("Comment"));
      prototype = comment->PrototypeTemplate();
      instance = comment->InstanceTemplate();
      instance->SetInternalFieldCount(2);

      // Document
      document->Inherit(node);
      document->SetClassName(v8::String::New("Document"));
      prototype = document->PrototypeTemplate();
      instance = document->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("doctype"), Document_doctype);
      instance->SetAccessor(v8::String::New("implementation"), Document_implementation);
      instance->SetAccessor(v8::String::New("documentElement"), Document_documentElement);
      prototype->Set(v8::String::New("createElement"), FunctionTemplate::New(Document_createElement), ReadOnly);
      prototype->Set(v8::String::New("createDocumentFragment"), FunctionTemplate::New(Document_createDocumentFragment), ReadOnly);
      prototype->Set(v8::String::New("createTextNode"), FunctionTemplate::New(Document_createTextNode), ReadOnly);
      prototype->Set(v8::String::New("createComment"), FunctionTemplate::New(Document_createComment), ReadOnly);
      prototype->Set(v8::String::New("createCDATASection"), FunctionTemplate::New(Document_createCDATASection), ReadOnly);
      prototype->Set(v8::String::New("createProcessingInstruction"), FunctionTemplate::New(Document_createProcessingInstruction), ReadOnly);
      prototype->Set(v8::String::New("createAttribute"), FunctionTemplate::New(Document_createAttribute), ReadOnly);
      prototype->Set(v8::String::New("createEntityReference"), FunctionTemplate::New(Document_createEntityReference), ReadOnly);
      prototype->Set(v8::String::New("getElementsByTagName"), FunctionTemplate::New(Document_getElementsByTagName), ReadOnly);
      prototype->Set(v8::String::New("importNode"), FunctionTemplate::New(Document_importNode), ReadOnly);
      prototype->Set(v8::String::New("createElementNS"), FunctionTemplate::New(Document_createElementNS), ReadOnly);
      prototype->Set(v8::String::New("createAttributeNS"), FunctionTemplate::New(Document_createAttributeNS), ReadOnly);
      prototype->Set(v8::String::New("getElementsByTagNameNS"), FunctionTemplate::New(Document_getElementsByTagNameNS), ReadOnly);
      prototype->Set(v8::String::New("getElementById"), FunctionTemplate::New(Document_getElementById), ReadOnly);
      #if defined (HAVE_DOMDOCUMENT_GETINPUTENCODING)
        instance->SetAccessor(v8::String::New("inputEncoding"), Document_inputEncoding);
      #endif
      #if defined (HAVE_DOMDOCUMENT_GETXMLENCODING)
        instance->SetAccessor(v8::String::New("xmlEncoding"), Document_xmlEncoding);
      #endif
      #if defined (HAVE_DOMDOCUMENT_GETXMLSTANDALONE)
        instance->SetAccessor(v8::String::New("xmlStandalone"), Document_xmlStandalone, Document_setXmlStandalone);
      #endif
      #if defined (HAVE_DOMDOCUMENT_GETXMLVERSION) && defined (HAVE_DOMDOCUMENT_SETXMLVERSION)
        instance->SetAccessor(v8::String::New("xmlVersion"), Document_xmlVersion, Document_setXmlVersion);
      #endif
      instance->SetAccessor(v8::String::New("strictErrorChecking"), Document_strictErrorChecking, Document_setStrictErrorChecking);
      instance->SetAccessor(v8::String::New("documentURI"), Document_documentURI, Document_setDocumentURI);
      prototype->Set(v8::String::New("adoptNode"), FunctionTemplate::New(Document_adoptNode), ReadOnly);
      #if defined (HAVE_DOMDOCUMENT_GETDOMCONFIG)
        instance->SetAccessor(v8::String::New("domConfig"), Document_domConfig);
      #endif
      prototype->Set(v8::String::New("normalizeDocument"), FunctionTemplate::New(Document_normalizeDocument), ReadOnly);
      prototype->Set(v8::String::New("renameNode"), FunctionTemplate::New(Document_renameNode), ReadOnly);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);
      instance->SetAccessor(v8::String::New("parentNode"), GetNull);
      instance->SetAccessor(v8::String::New("previousSibling"), GetNull);
      instance->SetAccessor(v8::String::New("nextSibling"), GetNull);

      // DocumentFragment
      documentfragment->Inherit(node);
      documentfragment->SetClassName(v8::String::New("DocumentFragment"));
      prototype = documentfragment->PrototypeTemplate();
      instance = documentfragment->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);

      // DocumentType
      documenttype->Inherit(node);
      documenttype->SetClassName(v8::String::New("DocumentType"));
      prototype = documenttype->PrototypeTemplate();
      instance = documenttype->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("name"), DocumentType_name);
      instance->SetAccessor(v8::String::New("entities"), DocumentType_entities);
      instance->SetAccessor(v8::String::New("notations"), DocumentType_notations);
      instance->SetAccessor(v8::String::New("publicId"), DocumentType_publicId);
      instance->SetAccessor(v8::String::New("systemId"), DocumentType_systemId);
      instance->SetAccessor(v8::String::New("internalSubset"), DocumentType_internalSubset);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);

      // Element
      element->Inherit(node);
      element->SetClassName(v8::String::New("Element"));
      prototype = element->PrototypeTemplate();
      instance = element->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("tagName"), Element_tagName);
      instance->SetAccessor(v8::String::New("attributes"), GetElementAttributes);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);
      prototype->Set(v8::String::New("getAttribute"), FunctionTemplate::New(Element_getAttribute), ReadOnly);
      prototype->Set(v8::String::New("setAttribute"), FunctionTemplate::New(Element_setAttribute), ReadOnly);
      prototype->Set(v8::String::New("removeAttribute"), FunctionTemplate::New(Element_removeAttribute), ReadOnly);
      prototype->Set(v8::String::New("getAttributeNode"), FunctionTemplate::New(Element_getAttributeNode), ReadOnly);
      prototype->Set(v8::String::New("setAttributeNode"), FunctionTemplate::New(Element_setAttributeNode), ReadOnly);
      prototype->Set(v8::String::New("removeAttributeNode"), FunctionTemplate::New(Element_removeAttributeNode), ReadOnly);
      prototype->Set(v8::String::New("getElementsByTagName"), FunctionTemplate::New(Element_getElementsByTagName), ReadOnly);
      prototype->Set(v8::String::New("getAttributeNS"), FunctionTemplate::New(Element_getAttributeNS), ReadOnly);
      prototype->Set(v8::String::New("setAttributeNS"), FunctionTemplate::New(Element_setAttributeNS), ReadOnly);
      prototype->Set(v8::String::New("removeAttributeNS"), FunctionTemplate::New(Element_removeAttributeNS), ReadOnly);
      prototype->Set(v8::String::New("getAttributeNodeNS"), FunctionTemplate::New(Element_getAttributeNodeNS), ReadOnly);
      prototype->Set(v8::String::New("setAttributeNodeNS"), FunctionTemplate::New(Element_setAttributeNodeNS), ReadOnly);
      prototype->Set(v8::String::New("getElementsByTagNameNS"), FunctionTemplate::New(Element_getElementsByTagNameNS), ReadOnly);
      prototype->Set(v8::String::New("hasAttribute"), FunctionTemplate::New(Element_hasAttribute), ReadOnly);
      prototype->Set(v8::String::New("hasAttributeNS"), FunctionTemplate::New(Element_hasAttributeNS), ReadOnly);
      #if defined (HAVE_DOMELEMENT_GETSCHEMATYPEINFO)
        instance->SetAccessor(v8::String::New("schemaTypeInfo"), Element_schemaTypeInfo);
      #endif
      prototype->Set(v8::String::New("setIdAttribute"), FunctionTemplate::New(Element_setIdAttribute), ReadOnly);
      prototype->Set(v8::String::New("setIdAttributeNS"), FunctionTemplate::New(Element_setIdAttributeNS), ReadOnly);
      prototype->Set(v8::String::New("setIdAttributeNode"), FunctionTemplate::New(Element_setIdAttributeNode), ReadOnly);

      // Entity
      entity->Inherit(node);
      entity->SetClassName(v8::String::New("Entity"));
      prototype = entity->PrototypeTemplate();
      instance = entity->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);
      instance->SetAccessor(v8::String::New("publicId"), Entity_publicId);
      instance->SetAccessor(v8::String::New("systemId"), Entity_systemId);
      instance->SetAccessor(v8::String::New("notationName"), Entity_notationName);
      #if defined (HAVE_DOMENTITY_GETINPUTENCODING)
        instance->SetAccessor(v8::String::New("inputEncoding"), Entity_inputEncoding);
      #endif
      #if defined (HAVE_DOMENTITY_GETXMLENCODING)
        instance->SetAccessor(v8::String::New("xmlEncoding"), Entity_xmlEncoding);
      #endif
      #if defined (HAVE_DOMENTITY_GETXMLVERSION)
        instance->SetAccessor(v8::String::New("xmlVersion"), Entity_xmlVersion);
      #endif

      // EntityReference
      entityreference->Inherit(node);
      entityreference->SetClassName(v8::String::New("EntityReference"));
      prototype = entityreference->PrototypeTemplate();
      instance = entityreference->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);

      // Notation
      notation->Inherit(node);
      notation->SetClassName(v8::String::New("Notation"));
      prototype = notation->PrototypeTemplate();
      instance = notation->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("publicId"), Notation_publicId);
      instance->SetAccessor(v8::String::New("systemId"), Notation_systemId);
      instance->SetAccessor(v8::String::New("nodeValue"), GetNull, SetModificationErr);

      // ProcessingInstruction
      processinginstruction->Inherit(node);
      processinginstruction->SetClassName(v8::String::New("ProcessingInstruction"));
      prototype = processinginstruction->PrototypeTemplate();
      instance = processinginstruction->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      instance->SetAccessor(v8::String::New("target"), ProcessingInstruction_target);
      instance->SetAccessor(v8::String::New("data"), ProcessingInstruction_data, ProcessingInstruction_setData);

      // Text
      text->Inherit(characterdata);
      text->SetClassName(v8::String::New("Text"));
      prototype = text->PrototypeTemplate();
      instance = text->InstanceTemplate();
      instance->SetInternalFieldCount(2);
      prototype->Set(v8::String::New("splitText"), FunctionTemplate::New(Text_splitText), ReadOnly);
      #if defined (HAVE_DOMTEXT_GETISELEMENTCONTENTWHITESPACE)
        instance->SetAccessor(v8::String::New("isElementContentWhitespace"), Text_isElementContentWhitespace);
      #endif
      instance->SetAccessor(v8::String::New("wholeText"), Text_wholeText);
      prototype->Set(v8::String::New("replaceWholeText"), FunctionTemplate::New(Text_replaceWholeText), ReadOnly);

      // CDATASection
      cdatasection->Inherit(text);
      cdatasection->SetClassName(v8::String::New("CDATASection"));
      prototype = cdatasection->PrototypeTemplate();
      instance = cdatasection->InstanceTemplate();
      instance->SetInternalFieldCount(2);
    }
    ~Templates() { ; }
};

static Templates *templates = (Templates*)0;
Templates *getTemplates() {
  if (!templates) {
    templates = new Templates;
  }
  return templates;
}

Handle<Value> _jsCreateNode(xercesc::DOMNode *xmlnode) {
  Local<Object> o;
  if (!xmlnode) {
    return Null();
  }
  switch(xmlnode->getNodeType()) {
    case xercesc::DOMNode::ELEMENT_NODE:
      o = getTemplates()->element->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::ATTRIBUTE_NODE:
      o = getTemplates()->attr->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::TEXT_NODE:
      o = getTemplates()->text->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::CDATA_SECTION_NODE:
      o = getTemplates()->cdatasection->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::ENTITY_REFERENCE_NODE:
      o = getTemplates()->entityreference->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::ENTITY_NODE:
      o = getTemplates()->entity->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::PROCESSING_INSTRUCTION_NODE:
      o = getTemplates()->processinginstruction->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::COMMENT_NODE:
      o = getTemplates()->comment->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::DOCUMENT_NODE:
      o = getTemplates()->document->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::DOCUMENT_TYPE_NODE:
      o = getTemplates()->documenttype->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::DOCUMENT_FRAGMENT_NODE:
      o = getTemplates()->documentfragment->GetFunction()->NewInstance();
    break;
    case xercesc::DOMNode::NOTATION_NODE:
      o = getTemplates()->notation->GetFunction()->NewInstance();
    break;
  }
  _jsNode(o, xmlnode);
  return o;
}

Handle<Value> _jsCreateDOMNodeList(xercesc::DOMNodeList *l) {
  Local<Function> NodeList = getTemplates()->nodelist->GetFunction();
  Local<Object> rval = NodeList->NewInstance();
  _jsNative(rval, External::New(l), XV8_NODE_LIST);
  return rval;
}

Handle<Value> _jsCreateDOMNamedNodeMap(xercesc::DOMNamedNodeMap *l) {
  Local<Function> NamedNodeMap = getTemplates()->namednodemap->GetFunction();
  Local<Object> rval = NamedNodeMap->NewInstance();
  _jsNative(rval, External::New(l), XV8_NAMED_NODE_MAP);
  return rval;
}

Handle<Value> _jsCreateDOMImplementation(xercesc::DOMImplementation *l) {
  Local<Function> DOMImplementation = getTemplates()->implementation->GetFunction();
  Local<Object> rval = DOMImplementation->NewInstance();
  _jsNative(rval, External::New(l), XV8_IMPLEMENTATION);
  return rval;
}

Handle<Value> _jsCreateDOMConfiguration(xercesc::DOMConfiguration *l) {
  Local<Function> DOMConfiguration = getTemplates()->configuration->GetFunction();
  Local<Object> rval = DOMConfiguration->NewInstance();
  _jsNative(rval, External::New(l), XV8_CONFIGURATION);
  return rval;
}

Handle<Value> _jsCreateDOMTypeInfo(const xercesc::DOMTypeInfo *l) {
  Local<Function> DOMTypeInfo = getTemplates()->typeinfo->GetFunction();
  Local<Object> rval = DOMTypeInfo->NewInstance();
  _jsNative(rval, External::New((void*)l), XV8_TYPE_INFO);
  return rval;
}

#if defined (DOM3) && defined (HAVE_DOMSTRINGLIST)
Handle<Value> _jsCreateDOMStringList(const xercesc::DOMStringList *l) {
  Local<Function> DOMStringList = getTemplates()->stringlist->GetFunction();
  Local<Object> rval = DOMStringList->NewInstance();
  _jsNative(rval, External::New((void*)l), XV8_STRING_LIST);
  return rval;
}
#endif

Handle<Value> _jsCreateDOMError(const xercesc::DOMError *e) {
  Local<Function> DOMError = getTemplates()->error->GetFunction();
  Local<Object> rval = DOMError->NewInstance();
  _jsNative(rval, External::New((void*)e), XV8_ERROR);
  return rval;
}

Handle<Value> _jsCreateDOMLocator(const xercesc::DOMLocator *l) {
  Local<Function> DOMLocator = getTemplates()->locator->GetFunction();
  Local<Object> rval = DOMLocator->NewInstance();
  _jsNative(rval, External::New((void*)l), XV8_LOCATION);
  return rval;
}

bool __domParseXML(Document *doc, const char *path, const char *srcpath = 0);
bool __domParseXML(Document *doc, const char *path, const char *srcpath) {
  bool broken = false;
  const char *userpath = srcpath ? srcpath : path;
  doc->err = (xercesc::ErrorHandler*) new xercesc::HandlerBase();
  doc->dom = new xercesc::XercesDOMParser();
  doc->dom->setValidationScheme(xercesc::XercesDOMParser::Val_Auto);
  doc->dom->setDoNamespaces(true);
  doc->dom->setErrorHandler(doc->err);
  try {
    doc->dom->parse(path);
  }
  catch (xercesc::XMLException const & ex) {
    char* message = xercesc::XMLString::transcode(ex.getMessage());
    std::cout << "Warning: XML exception: " << message <<  "parsing file " << userpath << std::endl;
    xercesc::XMLString::release(&message);
    broken = true;
  }
  catch (xercesc::DOMException const & ex) {
    char* message = xercesc::XMLString::transcode(ex.msg);
    std::cout << "Warning: DOM exception: " << message <<  "parsing file " << userpath << std::endl;
    xercesc::XMLString::release(&message);
    broken = true;
  }
  catch (xercesc::SAXParseException const & ex) {
    char* message = xercesc::XMLString::transcode(ex.getMessage());
    std::cout << "Warning: parser error " << message << " in " << userpath << " at line " << ex.getLineNumber() << ", column " << ex.getColumnNumber() << std::endl;
    xercesc::XMLString::release(&message);
    broken = true;
  }
  catch (...) {
    std::cout << "Warning: unexpected exception while parsing file " << userpath << std::endl;
    broken = true;
  }
  if (broken) {
    std::cout << "Warning: could not parse source file " << userpath << std::endl;
  }
  return !broken;
}

void setIdAttributeRecursive(xercesc::DOMNode *n, XMLCh *idattr) {
  if (n->getNodeType() == xercesc::DOMNode::ELEMENT_NODE) {
    xercesc::DOMElement *e = static_cast<xercesc::DOMElement*>(n);
    try {
      #if defined (HAVE_DOMELEMENT_SETIDATTRIBUTE_2ARG)
        e->setIdAttribute(idattr, true);
      #else
        e->setIdAttribute(idattr);
      #endif
    }
    catch (xercesc::DOMException const & ex) { ; }
  }
  xercesc::DOMNodeList *l = n->getChildNodes();
  XMLSize_t len = l->getLength();
  for (XMLSize_t i = 0; i < len; i++) {
    setIdAttributeRecursive(l->item(i), idattr);
  }
}

xv8::Document::Document(const char *path) :
  context(Context::New(NULL, getTemplates()->global)),
  context_scope(context) {
  this->path = path;
  this->dom = (xercesc::XercesDOMParser*)0;
  this->err = (xercesc::ErrorHandler*)0;
  if (!__domParseXML(this, path)) {
    throw new std::exception; // TODO
  }
  XMLCh *idattr = xercesc::XMLString::transcode("id");
  setIdAttributeRecursive(this->dom->getDocument(), idattr);
  xercesc::XMLString::release(&idattr);
  Handle<Value> document = _jsCreateNode(this->dom->getDocument());
  Local<Object> global = context->Global();
  global->Set(v8::String::New("document"), document, ReadOnly);
}

xv8::Document::~Document(void) {
  if (this->dom) {
    xercesc::DOMNode *doc = this->dom->getDocument();
    this->dom->adoptDocument();
    doc->release();
    delete this->dom;
  }
  if (this->err) {
    delete this->err;
  }
  context.Dispose();
}

bool has_initialized_xerces = false;
bool has_initialized_javascript = false;
class V8Wrap {
  public:
    v8::Persistent<v8::Context> context;
    v8::Context::Scope context_scope;
    v8::HandleScope handle_scope;
    V8Wrap() : context(Context::New()), context_scope(context) { ; }
    ~V8Wrap() { context.Dispose(); }
};
V8Wrap *v8wrap;

Document *xv8::Document::load(const char *path) {
  if (!has_initialized_xerces) {
    try {
      xercesc::XMLPlatformUtils::Initialize();
      has_initialized_xerces = true;
    }
    catch (const xercesc::XMLException& ex) {
      char* message = xercesc::XMLString::transcode(ex.getMessage());
      std::cout << "Error during initialization! :\n" << message << "\n";
      xercesc::XMLString::release(&message);
      has_initialized_xerces = false;
    }
  }
  if (!has_initialized_javascript) {
    v8wrap = new V8Wrap;
    has_initialized_javascript = true;
  }
  return new Document(path);
}

void *xv8::Document::release(xv8::Document **doc) {
  if (doc && *doc) {
    delete(*doc);
  }
  *doc = static_cast<xv8::Document*>(0);
}

// TODO: delete v8wrap at shutdown time.
