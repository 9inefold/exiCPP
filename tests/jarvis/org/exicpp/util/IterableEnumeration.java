//===- util/IterableEnumeration.java --------------------------------===//
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

package org.exicpp.util;

import java.util.Enumeration;
import java.util.Iterator;

/** From https://www.javaspecialists.eu/archive/Issue107-Making-Enumerations-Iterable.html */
public class IterableEnumeration<T> implements Iterable<T> {
  private final Enumeration<T> en;
  public IterableEnumeration() {
    this.en = new Enumeration<T>() {
      @Override public boolean hasMoreElements() { return false; }
      @Override public T nextElement() { return null; }
    }; 
  }
  public IterableEnumeration(Enumeration<T> en) {
    this.en = en;
  }
  // return an adaptor for the Enumeration
  public Iterator<T> iterator() {
    return new Iterator<T>() {
      public boolean hasNext() {
        return en.hasMoreElements();
      }
      public T next() {
        return en.nextElement();
      }
      public void remove() {
        throw new UnsupportedOperationException();
      }
    };
  }
  public static <T> Iterable<T> make(Enumeration<T> en) {
    return new IterableEnumeration<T>(en);
  }
}
