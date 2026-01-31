//===- xerces/impl/XMLEEDocumentFragmentScanner.java ----------------===//
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
import org.apache.xerces.impl.XMLDocumentFragmentScannerImpl;
import org.apache.xerces.util.AugmentationsImpl;
import org.apache.xerces.util.XMLStringBuffer;
import org.apache.xerces.xni.Augmentations;
import org.apache.xerces.xni.XNIException;
import org.exicpp.util.ReflectionHelpers;

public class XMLEEDocumentFragmentScanner
    extends XMLDocumentFragmentScannerImpl {
  
  /** String buffer. */
  private final XMLStringBuffer fStringBuffer = new XMLStringBuffer();

  /** fTempAugmentations */
  private static final Field rfTempAugmentations;
  static {
    rfTempAugmentations = getTempAugmentationsField();
    assert rfTempAugmentations != null;
  }

  @SuppressWarnings("unchecked")
  private Augmentations getTempAugmentations() {
    return (Augmentations) ReflectionHelpers.getObjectField(this, rfTempAugmentations);
  }
  private void setTempAugmentations(final Augmentations augs) {
    ReflectionHelpers.setObjectField(this, augs, rfTempAugmentations);
  }

  /**
   * Scans a character reference.
   * <p>
   * <pre>
   * [66] CharRef ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'
   * </pre>
   */
  protected void scanCharReference() throws IOException, XNIException {
    System.out.println("in scanCharReference");
    fStringBuffer.clear();
    int ch = scanCharReferenceValue(fStringBuffer, null);
    fMarkupDepth--;
    if (ch != -1) {
      // call handler
      if (fDocumentHandler != null) {
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
      }
    }
  } // scanCharReference()

  /** Gets the field fTempAugmentations with reflection */
  static Field getTempAugmentationsField() {
    try {
      Class<?> clazz = XMLDocumentFragmentScannerImpl.class;
      Field tempAugmentations = clazz.getDeclaredField("fTempAugmentations");
      tempAugmentations.setAccessible(true);
      return tempAugmentations;
    } catch (Exception e) {
      return null;
    }
  } // getTempAugmentationsField()
}
