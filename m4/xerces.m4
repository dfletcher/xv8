
AC_DEFUN([_XV8_CHECK_DOM], [
  AC_MSG_CHECKING([for DOM$1 in xerces])
  AC_TRY_LINK([
    #include <xercesc/dom/DOM.hpp>
  ], [
    $2
  ], [
    HAVE_DOM$1=yes
    AC_MSG_RESULT([yes])
  ], [
    AC_MSG_RESULT([no])
    HAVE_DOM$1=no
    AC_MSG_WARN([DOM level $1 not found, something is seriously wrong. Consider updating Xerces.])
  ])
  AC_MSG_CHECKING([if DOM$1 is enabled])
  ENABLE_DOM$1=yes
  AC_ARG_WITH([dom$1],[AS_HELP_STRING([--without-dom$1], [Disable DOM level $1 functionality. @<:@default=on@:>@])], [test "X$withval" = "Xno" && ENABLE_DOM$1=no], [])
  if test "X${HAVE_DOM$1}" = "Xyes" -a "X${ENABLE_DOM$1}" = "Xyes"; then
    AC_MSG_RESULT([yes])
    AC_DEFINE([DOM$1], [], [have DOM level $1 support])
  else
    AC_MSG_RESULT([no])
  fi
])

# (1) variable (2) message (3) code (4) action on success
AC_DEFUN([_XV8_XERCES_CHECK], [
  AC_MSG_CHECKING([for $2 in xerces])
  AC_TRY_LINK([
    #include <xercesc/dom/DOM.hpp>
  ], [
    $3
  ], [
    AC_DEFINE([$1], [], [xerces has $2])
    AC_MSG_RESULT([yes])
    $4
  ], [
    AC_MSG_RESULT([no])
    AC_MSG_WARN([No $2 in Xerces. Consider updating it.])
  ])
])

# (1) variable (2) message (3) qualified class name (4) action on success
AC_DEFUN([_XV8_XERCES_CHECK_CLASS], [
  _XV8_XERCES_CHECK([$1], [class $2], [$3 *ptr;], [$4])
])


AC_DEFUN([XV8_CHECK_XERCES], [

  XV8_MANDATORY_LIB([xerces-c], [main])

  # String size sanity check.
  AC_MSG_CHECKING([if sizeof(XMLCh) == sizeof(uint16_t)])
  AC_TRY_RUN([
    #include <stdint.h>
    #include <xercesc/dom/DOM.hpp>
    int main() {
      return ( sizeof(XMLCh) == sizeof(uint16_t) ) ? 0 : 1;
    }
  ], [
    AC_MSG_RESULT([yes])
  ], [
    AC_MSG_RESULT([no])
    AC_MSG_FAILURE([Xerces and V8 strings are not the same size, cannot continue.])
  ])

  _XV8_CHECK_DOM([1], [xercesc::DOMNode *n; n->getNodeType();])
  _XV8_CHECK_DOM([2], [xercesc::DOMNode *n; n->getNamespaceURI();])
  _XV8_CHECK_DOM([3], [xercesc::DOMNode *n; n->getTextContent();])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMSTRINGLIST], [DOMStringList], [xercesc::DOMStringList])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMCONFIGURATION], [DOMConfiguration], [xercesc::DOMConfiguration], [
    _XV8_XERCES_CHECK([HAVE_DOMCONFIGURATION_BOOLEAN], [DOMConfiguration boolean support], [
      xercesc::DOMConfiguration *c;
      XMLCh *x;
      bool b;
      c->setParameter(x, b);
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMLOCATOR], [DOMLocator], [xercesc::DOMLocator], [
    _XV8_XERCES_CHECK([HAVE_DOMLOCATOR_GETBYTEOFFSET], [DOMLocator::getByteOffset], [
      xercesc::DOMLocator *l;
      l->getByteOffset();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMLOCATOR_GETUTF16OFFSET], [DOMLocator::getUtf16Offset], [
      xercesc::DOMLocator *l;
      l->getUtf16Offset();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMLOCATOR_GETRELATEDNODE], [DOMLocator::getRelatedNode], [
      xercesc::DOMLocator *l;
      l->getRelatedNode();
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMNODE], [DOMLocator], [xercesc::DOMLocator], [
    _XV8_XERCES_CHECK([HAVE_DOMNODE_COMPAREDOCUMENTPOSITION], [DOMNode::compareDocumentPosition], [
      xercesc::DOMNode *l;
      l->compareDocumentPosition(l);
    ])
    _XV8_XERCES_CHECK([HAVE_DOMNODE_LOOKUPPREFIX], [DOMNode::lookupPrefix], [
      xercesc::DOMNode *l;
      const XMLCh *s;
      l->lookupPrefix(s);
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMELEMENT], [DOMElement], [xercesc::DOMElement], [
    _XV8_XERCES_CHECK([HAVE_DOMELEMENT_SETIDATTRIBUTE_2ARG], [DOMElement::setIdAttribute], [
      xercesc::DOMElement *e;
      XMLCh* a;
      bool b;
      e->setIdAttribute(a,b);
    ])
    _XV8_XERCES_CHECK([HAVE_DOMELEMENT_SETIDATTRIBUTENS_3ARG], [DOMElement::setIdAttributeNS], [
      xercesc::DOMElement *e;
      XMLCh *a, *b;
      bool c;
      e->setIdAttributeNS(a,b,c);
    ])
    _XV8_XERCES_CHECK([HAVE_DOMELEMENT_SETIDATTRIBUTENODE_2ARG], [DOMElement::setIdAttributeNode], [
      xercesc::DOMElement *e;
      xercesc::DOMAttr *a;
      bool b;
      e->setIdAttributeNode(a,b);
    ])
    _XV8_XERCES_CHECK([HAVE_DOMELEMENT_GETSCHEMATYPEINFO], [DOMElement::getSchemaTypeInfo], [
      xercesc::DOMElement *e;
      e->getSchemaTypeInfo();
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMATTR], [DOMAttr], [xercesc::DOMAttr], [
    _XV8_XERCES_CHECK([HAVE_DOMATTR_GETSCHEMATYPEINFO], [DOMAttr::getSchemaTypeInfo], [
      xercesc::DOMAttr *a;
      a->getSchemaTypeInfo();
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMDOCUMENT], [DOMDocument], [xercesc::DOMDocument], [
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_GETINPUTENCODING], [DOMDocument::getInputEncoding], [
      xercesc::DOMDocument *d;
      d->getInputEncoding();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_GETXMLENCODING], [DOMDocument::getXmlEncoding], [
      xercesc::DOMDocument *d;
      d->getXmlEncoding();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_GETXMLSTANDALONE], [DOMDocument::getXmlStandalone], [
      xercesc::DOMDocument *d;
      d->getXmlStandalone();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_SETXMLSTANDALONE], [DOMDocument::setXmlStandalone], [
      xercesc::DOMDocument *d;
      d->setXmlStandalone(true);
    ])
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_GETXMLVERSION], [DOMDocument::getXmlVersion], [
      xercesc::DOMDocument *d;
      d->getXmlVersion();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_SETXMLVERSION], [DOMDocument::setXmlVersion], [
      xercesc::DOMDocument *d;
      const XMLCh *a;
      d->setXmlVersion(a);
    ])
    _XV8_XERCES_CHECK([HAVE_DOMDOCUMENT_GETDOMCONFIG], [DOMDocument::getDOMConfig], [
      xercesc::DOMDocument *d;
      d->getDOMConfig();
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMENTITY], [DOMEntity], [xercesc::DOMEntity], [
    _XV8_XERCES_CHECK([HAVE_DOMENTITY_GETINPUTENCODING], [DOMEntity::getInputEncoding], [
      xercesc::DOMEntity *e;
      e->getInputEncoding();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMENTITY_GETXMLENCODING], [DOMEntity::getXmlEncoding], [
      xercesc::DOMEntity *e;
      e->getXmlEncoding();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMENTITY_GETXMLVERSION], [DOMEntity::getXmlVersion], [
      xercesc::DOMEntity *e;
      e->getXmlVersion();
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMTEXT], [DOMText], [xercesc::DOMText], [
    _XV8_XERCES_CHECK([HAVE_DOMTEXT_GETISELEMENTCONTENTWHITESPACE], [DOMText::getIsElementContentWhitespace], [
      xercesc::DOMText *t;
      t->getIsElementContentWhitespace();
    ])
  ])

  _XV8_XERCES_CHECK_CLASS([HAVE_DOMTYPEINFO], [DOMTypeInfo], [xercesc::DOMTypeInfo], [
    _XV8_XERCES_CHECK([HAVE_DOMTYPEINFO_GETTYPENAME], [DOMTypeInfo::getTypeName], [
      xercesc::DOMTypeInfo *t;
      t->getTypeName();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMTYPEINFO_GETTYPENAMESPACE], [DOMTypeInfo::getTypeNamespace], [
      xercesc::DOMTypeInfo *t;
      t->getTypeNamespace();
    ])
    _XV8_XERCES_CHECK([HAVE_DOMTYPEINFO_ISDERIVEDFROM], [DOMTypeInfo::isDerivedFrom], [
      xercesc::DOMTypeInfo *t;
      const XMLCh *a, *b;
      t->isDerivedFrom(a, b, xercesc::DOMTypeInfo::DERIVATION_LIST);
    ])
  ])

  # Some versions of Xerces want argument #5 of DOMUserDataHandler::handle() to
  # be const, but others do not. The C++ compiler sees these as different sigs,
  # so in order to write one function that works on all xerces, the following
  # DOMUSERDATAHANDLER_ARG5_CONST macro is used in place of "const" there.
  AC_MSG_CHECKING([DOMUserDataHandler callback style])
  AC_TRY_LINK([
    #include <xercesc/dom/DOM.hpp>
  ], [
    class CheckDOMUserDataHandler : public xercesc::DOMUserDataHandler { public: void handle (xercesc::DOMUserDataHandler::DOMOperationType operation, const XMLCh* key, void *data, const xercesc::DOMNode *src, const xercesc::DOMNode *dst) { ; } };
    new CheckDOMUserDataHandler;
  ], [
    AC_DEFINE([DOMUSERDATAHANDLER_ARG5_CONST], [const], [DOMUserDataHandler callback style])
    AC_MSG_RESULT([const])
  ], [
    AC_TRY_LINK([
      #include <xercesc/dom/DOM.hpp>
    ], [
      class CheckDOMUserDataHandler : public xercesc::DOMUserDataHandler { public: void handle (xercesc::DOMUserDataHandler::DOMOperationType operation, const XMLCh* key, void *data, const xercesc::DOMNode *src, xercesc::DOMNode *dst) { ; } };
      new CheckDOMUserDataHandler;
    ], [
      AC_DEFINE([DOMUSERDATAHANDLER_ARG5_CONST], [], [DOMUserDataHandler callback style])
      AC_MSG_RESULT([non-const])
    ], [
      AC_MSG_FAILURE([Could not detect the callback style of DOMUserDataHandler, sorry. Please report a bug along that includes Xerces-C++ version number.])
    ])
  ])

])
