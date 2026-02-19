//===- xerces/impl/XMLEEScannerCommon.java --------------------------===//
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

import java.lang.IllegalArgumentException;
import java.lang.reflect.Field;

class XMLEEScannerCommon {
  /** The default class type (XMLDocumentFragmentScannerImpl) */
  public static final Class<? extends XMLScanner>
      DEFAULT_SCANNER_CLAZZ = XMLDocumentFragmentScannerImpl.class;

  /** The class for the given scanner. */
  protected final Class<? extends XMLScanner> fClazz;
  /** The instance of the class. */
  protected XMLScanner fScanner = null;

  public XMLEEScannerCommon(Class<? extends XMLScanner> clazz) {
    this(null, clazz);
  }

  XMLEEScannerCommon(XMLScanner scanner, Class<? extends XMLScanner> clazz) {
    fClazz = clazz;
    if (scanner != null && isSimilar(scanner))
      fScanner = scanner;
  }

  /** Creates a defaulted instance (XMLDocumentFragmentScannerImpl) */
  public static XMLEEScannerCommon create() {
    return new XMLEEScannerCommon(null, DEFAULT_SCANNER_CLAZZ);
  }
  /** Creates a defaulted instance (XMLDocumentFragmentScannerImpl) */
  public static XMLEEScannerCommon create(XMLScanner scanner) {
    return new XMLEEScannerCommon(scanner, DEFAULT_SCANNER_CLAZZ);
  }

  public Field getParentField(final String name) {
    try {
      Field tempAugmentations = fClazz.getDeclaredField(name);
      tempAugmentations.setAccessible(true);
      return tempAugmentations;
    } catch (Exception e) {
      return null;
    }
  } // getParentField()

  public Field getParentFieldChk(final String name) {
    Field out = getParentField(name);
    assert out != null;
    return out;
  } // getParentFieldChk()

  public XMLEEScannerCommon newFromThis(XMLScanner scanner)
      throws IllegalArgumentException {
    assertSimilar(scanner);
    return new XMLEEScannerCommon(scanner, this.fClazz);
  } // newFromThis(XMLScanner)

  void setScanner(XMLScanner scanner) throws IllegalArgumentException {
    assertSimilar(scanner);
    fScanner = scanner;
  } // setScanner(XMLScanner)

  /** Checks if a scanner is bound */
  public boolean hasScanner() { return fScanner != null; }

  /** Checks if a scanner is a similar type */
  public boolean isSimilar(XMLScanner scanner) {
    return fClazz.isAssignableFrom(scanner.getClass());
  } // isSimilar(XMLScanner)

  /** Ensures a scanner is a similar type */
  protected void assertSimilar(XMLScanner scanner)
      throws IllegalArgumentException {
    if (scanner == null)
      throw new NullPointerException("scanner cannot be null!");
    // Ensure similarity
    if (!isSimilar(scanner)) {
      // Can only rebind the current class type.
      throw new IllegalArgumentException(
          "invalid scanner class: " + scanner.getClass().getTypeName() +
          " does not extend " + fClazz.getTypeName());
    }
  } // assertSimilar(XMLScanner)
}
