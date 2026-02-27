//===- openexi/DTDBodyHandler.java ----------------------------------===//
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

package org.exicpp.sax;

import org.xml.sax.SAXException;

/** Implements one function: dtdBody(String text) */
public interface DTDBodyHandler {
  /**
   * The DTD body.
   *
   * @param text The text found in the DTD body.
   *
   * @throws SAXException Thrown by handler to signal an error.
   */
  public void dtdBody(String text) throws SAXException;
}
