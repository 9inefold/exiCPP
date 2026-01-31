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
import org.apache.xerces.util.AugmentationsImpl;
import org.apache.xerces.util.XMLChar;
import org.apache.xerces.util.XMLStringBuffer;
import org.apache.xerces.xni.Augmentations;
import org.apache.xerces.xni.XMLString;
import org.apache.xerces.xni.XNIException;
import org.exicpp.util.ReflectionHelpers;

public class XMLEENSDocumentScanner extends XMLNSDocumentScannerImpl {
  
  /** String buffer. */
  private final XMLStringBuffer fStringBuffer = new XMLStringBuffer();
  /** String buffer. */
  private final XMLStringBuffer fStringBuffer2 = new XMLStringBuffer();

  /** fTempAugmentations */
  private static final Field rfTempAugmentations;
  static {
    rfTempAugmentations = getParentField("fTempAugmentations");
    assert rfTempAugmentations != null;
  }

  @SuppressWarnings("unchecked")
  private Augmentations getTempAugmentations() {
    return (Augmentations) ReflectionHelpers.getObjectField(this, rfTempAugmentations);
  }
  private void setTempAugmentations(final Augmentations augs) {
    ReflectionHelpers.setObjectField(this, augs, rfTempAugmentations);
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
    System.out.println("in scanCharReference: " + getStringFromXML(fStringBuffer));
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
      final Augmentations tempAugs = getTempAugmentations();
      if (tempAugs != null) {
        tempAugs.removeAllItems();
        augs = tempAugs;
      } else {
        augs = new AugmentationsImpl();
        setTempAugmentations(augs);
      }
      augs.putItem(Constants.CHAR_REF_PROBABLE_WS, Boolean.TRUE);
    }
    fDocumentHandler.characters(fStringBuffer, augs);
    if (fNotifyCharRefs) {
      fDocumentHandler.endGeneralEntity(fCharRefLiteral, null);
    }
  } // scanCharReference()

  private static Field getParentField(final String name) {
    try {
      Class<?> clazz = XMLDocumentFragmentScannerImpl.class;
      Field tempAugmentations = clazz.getDeclaredField("fTempAugmentations");
      tempAugmentations.setAccessible(true);
      return tempAugmentations;
    } catch (Exception e) {
      return null;
    }
  } // getParentField()
}
