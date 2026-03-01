//===- xerces/impl/XMLEENSDocumentScanner.java ----------------------===//
//
// Copyright (C) 2026 Ninefold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//

package org.apache.xerces.impl;

import java.io.IOException;
import java.lang.reflect.Field;
import org.apache.xerces.impl.XMLNSDocumentScannerImpl;
import org.apache.xerces.impl.XMLDocumentFragmentScannerImpl;
import org.apache.xerces.impl.msg.XMLMessageFormatter;
import org.apache.xerces.util.AugmentationsImpl;
import org.apache.xerces.util.XMLAttributesImpl;
import org.apache.xerces.util.XMLChar;
import org.apache.xerces.util.XMLStringBuffer;
import org.apache.xerces.util.XMLSymbols;
import org.apache.xerces.xni.Augmentations;
import org.apache.xerces.xni.NamespaceContext;
import org.apache.xerces.xni.XMLAttributes;
import org.apache.xerces.xni.XMLString;
import org.apache.xerces.xni.XNIException;
import org.apache.xerces.impl.XMLEEScannerCommonImpl;
import org.exicpp.util.ReflectionHelpers;

public class XMLEENSDocumentScanner extends XMLNSDocumentScannerImpl {
  /** String buffer. */
  private final XMLStringBuffer fStringBuffer = new XMLStringBuffer();
  /** String buffer. */
  private final XMLStringBuffer fStringBuffer2 = new XMLStringBuffer();
  /** String buffer. */
  private final XMLStringBuffer fStringBuffer3 = new XMLStringBuffer();
  /** String buffer. */
  private final XMLStringBuffer fStringBufferX = new XMLStringBuffer();

  /** Fields */
  private final XMLEEScannerCommonImpl thiz;
  public XMLEENSDocumentScanner() {
    super();
    thiz = new XMLEEScannerCommonImpl(this);
  }

  static String getStringFromXML(XMLString xstr) {
    return new String(xstr.ch, xstr.offset, xstr.length);
  }

  /**
   * Scans a character reference.
   * <p>
   * <pre>
   * [66] CharRef ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'
   * </pre>
   */
  @Override
  protected void scanCharReference() throws IOException, XNIException {
    fStringBuffer.clear(); fStringBuffer2.clear();
    fStringBuffer.append("&#");
    int ch = scanCharReferenceValue(fStringBuffer2, fStringBuffer);
    //System.out.println("in scanCharReference: " + getStringFromXML(fStringBuffer));
    fMarkupDepth--;
    if (ch == -1 || fDocumentHandler == null)
      return;
    // call handler
    if (fNotifyCharRefs) {
      fDocumentHandler.startGeneralEntity(fCharRefLiteral, null, null,
                                          null);
    }
    Augmentations augs = null;
    if (fValidation && ch <= 0x20) {
      final Augmentations tempAugs = thiz.getTempAugmentations();
      if (tempAugs != null) {
        tempAugs.removeAllItems();
        augs = tempAugs;
      } else {
        augs = new AugmentationsImpl();
        thiz.setTempAugmentations(augs);
      }
      augs.putItem(Constants.CHAR_REF_PROBABLE_WS, Boolean.TRUE);
    }
    fDocumentHandler.characters(fStringBuffer, augs);
    if (fNotifyCharRefs) {
      fDocumentHandler.endGeneralEntity(fCharRefLiteral, null);
    }
  } // scanCharReference()

  /** HACK: Needs updating to work with entities */
  static private final boolean ATT_SWAP_HACK = true;

  static private boolean isBuiltinSymbol(String entityName) {
    return entityName != null && (
      entityName == fAmpSymbol  ||
      entityName == fAposSymbol ||
      entityName == fLtSymbol   ||
      entityName == fGtSymbol   ||
      entityName == fQuotSymbol);
  }

  /* Stuff */
  private boolean scanAttributeValueImpl(XMLString value, XMLString nonNormalizedValue, String atName,
                                         boolean checkEntities, String eleName)
      throws IOException, XNIException {
    // quote
    int quote = fEntityScanner.peekChar();
    if (quote != '\'' && quote != '"') {
      reportFatalError("OpenQuoteExpected", new Object[]{eleName, atName});
    }

    fEntityScanner.scanChar();
    int entityDepth = fEntityDepth;

    int c = fEntityScanner.scanLiteral(quote, value);
    int fromIndex = 0;
    if (c == quote && (fromIndex = isUnchangedByNormalization(value)) == -1) {
      /** Both the non-normalized and normalized attribute values are equal. **/
      nonNormalizedValue.setValues(value);
      int cquote = fEntityScanner.scanChar();
      if (cquote != quote) {
        reportFatalError("CloseQuoteExpected", new Object[]{eleName, atName});
      }
      return true;
    }
    fStringBuffer2.clear();
    fStringBuffer2.append(value);
    normalizeWhitespace(value, fromIndex);
    if (c != quote) {
      fScanningAttribute = true;
      fStringBuffer.clear();
      do {
        fStringBuffer.append(value);
        if (c == '&') {
          fEntityScanner.skipChar('&');
          if (entityDepth == fEntityDepth) {
            fStringBuffer.append('&');
            fStringBuffer2.append('&');
          }
          if (fEntityScanner.skipChar('#')) {
            if (entityDepth == fEntityDepth) {
              fStringBuffer.append('#');
              fStringBuffer2.append('#');
            }
            fStringBuffer3.clear(); fStringBufferX.clear();
            scanCharReferenceValue(fStringBuffer3, fStringBufferX);
            fStringBuffer.append(fStringBufferX);
            fStringBuffer2.append(fStringBufferX);
          } else {
            String entityName = fEntityScanner.scanName();
            final boolean isBuiltin = isBuiltinSymbol(entityName);
            if (entityName == null) {
              reportFatalError("NameRequiredInReference", null);
            } else if (entityDepth == fEntityDepth) {
              if (isBuiltin)
                fStringBuffer.append(entityName);
              fStringBuffer2.append(entityName);
            }
            if (!fEntityScanner.skipChar(';')) {
              reportFatalError("SemicolonRequiredInReference", new Object[]{entityName});
            } else if (entityDepth == fEntityDepth) {
              if (isBuiltin)
                fStringBuffer.append(';');
              fStringBuffer2.append(';');
            }
            if (!isBuiltin) {
              if (fEntityManager.isExternalEntity(entityName)) {
                reportFatalError("ReferenceToExternalEntity", new Object[]{entityName});
              } else {
                if (!fEntityManager.isDeclaredEntity(entityName)) {
                  // WFC & VC: Entity Declared
                  if (checkEntities) {
                    if (fValidation) {
                      fErrorReporter.reportError(
                          XMLMessageFormatter.XML_DOMAIN, "EntityNotDeclared",
                          new Object[]{entityName}, XMLErrorReporter.SEVERITY_ERROR);
                    }
                  } else {
                    reportFatalError("EntityNotDeclared", new Object[]{entityName});
                  }
                }
                fEntityManager.startEntity(entityName, true);
              }
            }
          }
        } else if (c == '<') {
          reportFatalError("LessthanInAttValue", new Object[]{eleName, atName});
          fEntityScanner.scanChar();
          if (entityDepth == fEntityDepth) {
            fStringBuffer2.append((char)c);
          }
        } else if (c == '%' || c == ']') {
          fEntityScanner.scanChar();
          fStringBuffer.append((char)c);
          if (entityDepth == fEntityDepth) {
            fStringBuffer2.append((char)c);
          }
        } else if (c == '\n' || c == '\r') {
          fEntityScanner.scanChar();
          fStringBuffer.append(' ');
          if (entityDepth == fEntityDepth) {
            fStringBuffer2.append('\n');
          }
        } else if (c != -1 && XMLChar.isHighSurrogate(c)) {
          fStringBuffer3.clear();
          if (scanSurrogates(fStringBuffer3)) {
            fStringBuffer.append(fStringBuffer3);
            if (entityDepth == fEntityDepth) {
              fStringBuffer2.append(fStringBuffer3);
            }
          }
        } else if (c != -1 && isInvalidLiteral(c)) {
          reportFatalError("InvalidCharInAttValue",
                           new Object[]{eleName, atName, Integer.toString(c, 16)});
          fEntityScanner.scanChar();
          if (entityDepth == fEntityDepth) {
            fStringBuffer2.append((char)c);
          }
        }
        c = fEntityScanner.scanLiteral(quote, value);
        if (entityDepth == fEntityDepth) {
          fStringBuffer2.append(value);
        }
        normalizeWhitespace(value);
      } while (c != quote || entityDepth != fEntityDepth);
      fStringBuffer.append(value);
      value.setValues(fStringBuffer);
      fScanningAttribute = false;
    }
    nonNormalizedValue.setValues(fStringBuffer2);

    // quote
    int cquote = fEntityScanner.scanChar();
    if (cquote != quote) {
      reportFatalError("CloseQuoteExpected", new Object[]{eleName, atName});
    }
    return nonNormalizedValue.equals(value.ch, value.offset, value.length);
  }

  /**
   * Scans an attribute value and normalizes whitespace converting all
   * whitespace characters to space characters.
   *
   * [10] AttValue ::= '"' ([^<&"] | Reference)* '"' | "'" ([^<&'] | Reference)* "'"
   *
   * @param value The XMLString to fill in with the value.
   * @param nonNormalizedValue The XMLString to fill in with the
   *                           non-normalized value.
   * @param atName The name of the attribute being parsed (for error msgs).
   * @param checkEntities true if undeclared entities should be reported as VC violation,
   *                      false if undeclared entities should be reported as WFC violation.
   * @param eleName The name of element to which this attribute belongs.
   *
   * @return true if the non-normalized and normalized value are the same
   *
   * <strong>Note:</strong> This method uses fStringBuffer2, anything in it
   * at the time of calling is lost.
   **/
  @Override
  protected boolean scanAttributeValue(XMLString value, XMLString nonNormalizedValue, String atName,
                                       boolean checkEntities, String eleName)
      throws IOException, XNIException {
    if (ATT_SWAP_HACK)
      return super.scanAttributeValue(nonNormalizedValue, value, atName, checkEntities, eleName);
    else
      return scanAttributeValueImpl(value, nonNormalizedValue, atName, checkEntities, eleName);
  } // scanAttributeValue()

  /**
   * Scans an attribute.
   * <p>
   * <pre>
   * [41] Attribute ::= Name Eq AttValue
   * </pre>
   * <p>
   * <strong>Note:</strong> This method assumes that the next
   * character on the stream is the first character of the attribute
   * name.
   * <p>
   * <strong>Note:</strong> This method uses the fAttributeQName and
   * fQName variables. The contents of these variables will be
   * destroyed.
   *
   * @param attributes The attributes list for the scanned attribute.
   */
  @Override
  protected void scanAttribute(XMLAttributesImpl attributes)
      throws IOException, XNIException {

    // name
    if (fEntityScanner.peekChar() != ':')
      // Normal scanning
      fEntityScanner.scanQName(fAttributeQName);
    else {
      final String name = fEntityScanner.scanName();
      final String localpart = name.substring(1);
      fAttributeQName.setValues("", localpart, name, null);
    }

    // equals
    fEntityScanner.skipSpaces();
    if (!fEntityScanner.skipChar('=')) {
      reportFatalError(
          "EqRequiredInAttribute",
          new Object[] {fCurrentElement.rawname, fAttributeQName.rawname});
    }
    fEntityScanner.skipSpaces();

    // content
    int attrIndex;

    if (fBindNamespaces) {
      attrIndex = attributes.getLength();
      attributes.addAttributeNS(fAttributeQName, XMLSymbols.fCDATASymbol, null);
    } else {
      int oldLen = attributes.getLength();
      attrIndex =
          attributes.addAttribute(fAttributeQName, XMLSymbols.fCDATASymbol, null);

      // WFC: Unique Att Spec
      if (oldLen == attributes.getLength()) {
        reportFatalError(
            "AttributeNotUnique",
            new Object[] {fCurrentElement.rawname, fAttributeQName.rawname});
      }
    }

    // Scan attribute value and return true if the non-normalized and normalized
    // value are the same
    boolean isSameNormalizedAttr = scanAttributeValue(
        this.fTempString, fTempString2, fAttributeQName.rawname,
        fIsEntityDeclaredVC, fCurrentElement.rawname);

    String value = fTempString.toString();
    attributes.setValue(attrIndex, value);
    // If the non-normalized and normalized value are the same, avoid creating a
    // new string.
    if (!isSameNormalizedAttr) {
      attributes.setNonNormalizedValue(attrIndex, fTempString2.toString());
    }
    attributes.setSpecified(attrIndex, true);

    // record namespace declarations if any.
    if (fBindNamespaces) {

      String localpart = fAttributeQName.localpart;
      String prefix = fAttributeQName.prefix != null ? fAttributeQName.prefix
                                                     : XMLSymbols.EMPTY_STRING;
      // when it's of form xmlns="..." or xmlns:prefix="...",
      // it's a namespace declaration. but prefix:xmlns="..." isn't.
      if (prefix == XMLSymbols.PREFIX_XMLNS ||
          prefix == XMLSymbols.EMPTY_STRING &&
              localpart == XMLSymbols.PREFIX_XMLNS) {

        // get the internalized value of this attribute
        String uri = fSymbolTable.addSymbol(value);

        // 1. "xmlns" can't be bound to any namespace
        if (prefix == XMLSymbols.PREFIX_XMLNS &&
            localpart == XMLSymbols.PREFIX_XMLNS) {
          fErrorReporter.reportError(XMLMessageFormatter.XMLNS_DOMAIN,
                                     "CantBindXMLNS",
                                     new Object[] {fAttributeQName},
                                     XMLErrorReporter.SEVERITY_FATAL_ERROR);
        }

        // 2. the namespace for "xmlns" can't be bound to any prefix
        if (uri == NamespaceContext.XMLNS_URI) {
          fErrorReporter.reportError(XMLMessageFormatter.XMLNS_DOMAIN,
                                     "CantBindXMLNS",
                                     new Object[] {fAttributeQName},
                                     XMLErrorReporter.SEVERITY_FATAL_ERROR);
        }

        // 3. "xml" can't be bound to any other namespace than it's own
        if (localpart == XMLSymbols.PREFIX_XML) {
          if (uri != NamespaceContext.XML_URI) {
            fErrorReporter.reportError(XMLMessageFormatter.XMLNS_DOMAIN,
                                       "CantBindXML",
                                       new Object[] {fAttributeQName},
                                       XMLErrorReporter.SEVERITY_FATAL_ERROR);
          }
        }
        // 4. the namespace for "xml" can't be bound to any other prefix
        else {
          if (uri == NamespaceContext.XML_URI) {
            fErrorReporter.reportError(XMLMessageFormatter.XMLNS_DOMAIN,
                                       "CantBindXML",
                                       new Object[] {fAttributeQName},
                                       XMLErrorReporter.SEVERITY_FATAL_ERROR);
          }
        }

        prefix = localpart != XMLSymbols.PREFIX_XMLNS ? localpart
                                                      : XMLSymbols.EMPTY_STRING;

        // http://www.w3.org/TR/1999/REC-xml-names-19990114/#dt-prefix
        // We should only report an error if there is a prefix,
        // that is, the local part is not "xmlns". -SG
        if (uri == XMLSymbols.EMPTY_STRING &&
            localpart != XMLSymbols.PREFIX_XMLNS) {
          fErrorReporter.reportError(XMLMessageFormatter.XMLNS_DOMAIN,
                                     "EmptyPrefixedAttName",
                                     new Object[] {fAttributeQName},
                                     XMLErrorReporter.SEVERITY_FATAL_ERROR);
        }

        // declare prefix in context
        fNamespaceContext.declarePrefix(prefix, uri.length() != 0 ? uri : null);
        // bind namespace attribute to a namespace
        attributes.setURI(attrIndex,
                          fNamespaceContext.getURI(XMLSymbols.PREFIX_XMLNS));

      } else {
        // attempt to bind attribute
        if (fAttributeQName.prefix != null) {
          attributes.setURI(attrIndex,
                            fNamespaceContext.getURI(fAttributeQName.prefix));
        }
      }
    }
  } // scanAttribute(XMLAttributes)

  /**
   * Scans an entity reference.
   *
   * @throws IOException  Thrown if i/o error occurs.
   * @throws XNIException Thrown if handler throws exception upon
   *                      notification.
   */
  protected void scanEntityReference() throws IOException, XNIException {
    // name
    String name = fEntityScanner.scanName();
    if (name == null) {
      reportFatalError("NameRequiredInReference", null);
      return;
    }

    // end
    if (!fEntityScanner.skipChar(';')) {
      reportFatalError("SemicolonRequiredInReference", new Object[]{name});
    }
    fMarkupDepth--;

    // handle built-in entities
    if (isBuiltinSymbol(name)) {
      fStringBufferX.clear();
      fStringBufferX.append('&');
      fStringBufferX.append(name);
      fStringBufferX.append(';');
      fTempString.setValues(fStringBufferX);
      fDocumentHandler.characters(fTempString, null);
    }
    // start general entity
    else if (fEntityManager.isUnparsedEntity(name)) {
      reportFatalError("ReferenceToUnparsedEntity", new Object[]{name});
    } else {
      if (!fEntityManager.isDeclaredEntity(name)) {
        if (fIsEntityDeclaredVC) {
          if (fValidation)
            fErrorReporter.reportError(XMLMessageFormatter.XML_DOMAIN,
                                       "EntityNotDeclared", new Object[]{name},
                                       XMLErrorReporter.SEVERITY_ERROR);
        } else {
          reportFatalError("EntityNotDeclared", new Object[]{name});
        }
      }
      fEntityManager.startEntity(name, false);
    }
  } // scanEntityReference()
}
