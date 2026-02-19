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
import org.apache.xerces.util.XMLChar;
import org.apache.xerces.util.XMLStringBuffer;
import org.apache.xerces.util.XMLSymbols;
import org.apache.xerces.xni.Augmentations;
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
  XMLEENSDocumentScanner() {
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
}
