//===- xerces/impl/XMLEEScannerCommonImpl.java ----------------------===//
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

import java.lang.reflect.Field;
import org.apache.xerces.xni.Augmentations;
import org.exicpp.util.ReflectionHelpers;

class XMLEEScannerCommonImpl extends XMLEEScannerCommon {
  /** fTempAugmentations */
  static final Field rfTempAugmentations;
  static {
    var rthis = XMLEEScannerCommon.create();
    rfTempAugmentations = rthis.getParentFieldChk("fTempAugmentations");
  }

  public XMLEEScannerCommonImpl(XMLDocumentFragmentScannerImpl scanner) {
    super(scanner, XMLEEScannerCommon.DEFAULT_SCANNER_CLAZZ);
    assert scanner != null;
  }

  @SuppressWarnings("unchecked")
  public Augmentations getTempAugmentations() {
    return (Augmentations) ReflectionHelpers.getObjectField(super.fScanner, rfTempAugmentations);
  }
  public void setTempAugmentations(final Augmentations augs) {
    ReflectionHelpers.setObjectField(super.fScanner, augs, rfTempAugmentations);
  }
}
