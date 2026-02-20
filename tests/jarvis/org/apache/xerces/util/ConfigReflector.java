//===- xerces/util/ConfigReflector.java -----------------------------===//
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
///
/// Implements a utility to reflect on the contents of xerces'
/// ParserConfigurationSettings class.
///
//===----------------------------------------------------------------===//

package org.apache.xerces.util;

import java.util.ArrayList;
import java.util.HashMap;
import java.lang.NullPointerException;
import java.lang.reflect.Field;
//import javax.xml.parsers.SAXParser;
import org.xml.sax.XMLReader;

import org.apache.xerces.impl.Constants;
import org.apache.xerces.jaxp.SAXParserImpl;
import org.apache.xerces.jaxp.SAXParserImpl2;
import org.apache.xerces.util.ParserConfigurationSettings;
import org.apache.xerces.xni.parser.XMLComponentManager;
import org.apache.xerces.xni.parser.XMLConfigurationException;
import org.exicpp.util.CircularReferenceException;
import org.exicpp.util.ReflectionHelpers;
import org.exicpp.util.XConstants;

public class ConfigReflector {
  public static final String CONFIG_REFLECTOR =
      XConstants.EXICPP_PROPERTY_PREFIX + XConstants.CONFIG_REFLECTOR_PROPERTY;
  
  // Holder implementation

  private static final String[] EMPTY_CONFIG_ARRAY = {};

  public static enum Kind {
    UnknownKind,
    PCSKind,
  }

  /** Unknown */
  static class Holder {
    protected final Object fReader;
    protected final Kind fKind;

    public Holder(Object reader) { this(reader, Kind.UnknownKind); }
    protected Holder(Object reader, Kind K) {
      fReader = reader;
      fKind = K;
    }

    final Kind kind() {
      return fKind;
    }
    String[] getRecognizedFeatures() {
      return EMPTY_CONFIG_ARRAY;
    }
    String[] getRecognizedProperties() {
      return EMPTY_CONFIG_ARRAY;
    }
  }

  /** ParserConfigurationSettings */
  private static class PCSHolder extends Holder {
    final Field _fConfiguration;
    public PCSHolder(org.apache.xerces.parsers.SAXParser reader, Field _fConfiguration) {
      super(reader, Kind.PCSKind);
      this._fConfiguration = _fConfiguration;
    }

    @SuppressWarnings("unchecked")
    private org.apache.xerces.parsers.SAXParser getReader() {
      return (org.apache.xerces.parsers.SAXParser)fReader;
    }

    @SuppressWarnings("unchecked")
    private ParserConfigurationSettings getfConfiguration() {
      var fConfiguration =
        ReflectionHelpers.getObjectField(fReader, _fConfiguration);
      if (fConfiguration instanceof ParserConfigurationSettings)
        return (ParserConfigurationSettings)fConfiguration;
      return null;
    }

    // TODO: Get parent settings

    @Override
    String[] getRecognizedFeatures() {
      var fConfiguration = getfConfiguration();
      if (fConfiguration == null)
        return EMPTY_CONFIG_ARRAY;
      try {
        Field f = ParserConfigurationSettings.class.getDeclaredField("fRecognizedFeatures");
        f.setAccessible(true);
        @SuppressWarnings("unchecked")
        var fRecognizedFeatures = (ArrayList)f.get(fConfiguration);
        return (String[]) fRecognizedFeatures.toArray();
      } catch (Exception e) {
        return EMPTY_CONFIG_ARRAY;
      }
    }

    @Override
    String[] getRecognizedProperties() {
      var fConfiguration = getfConfiguration();
      if (fConfiguration == null)
        return EMPTY_CONFIG_ARRAY;
      try {
        Field f = ParserConfigurationSettings.class.getDeclaredField("fRecognizedProperties");
        f.setAccessible(true);
        @SuppressWarnings("unchecked")
        var fRecognizedProperties = (ArrayList)f.get(fConfiguration);
        return (String[]) fRecognizedProperties.toArray();
      } catch (Exception e) {
        return EMPTY_CONFIG_ARRAY;
      }
    }
  }

  // Class implementation

  /** Ensures the same class isn't registered multiple times. */
  private static final class RecursionChecker {
    private final ArrayList<Object> readers = new ArrayList<>();
    public void check(Object reader) throws CircularReferenceException {
      if (reader == null)
        throw new NullPointerException(
          "null reader found when creating holder");
      if (readers.contains(reader))
        throw new CircularReferenceException(
          "circular reference found when creating holder");
      readers.add(reader);
    }
  }

  private final Holder fHolder;

  public ConfigReflector(XMLReader reader) throws CircularReferenceException {
    // Now dispatch the class
    fHolder = createHolder(reader, new RecursionChecker());
  }
  public ConfigReflector(javax.xml.parsers.SAXParser reader)
      throws CircularReferenceException {
    // Now dispatch the class
    fHolder = createHolder(reader, new RecursionChecker());
  }

  private static Holder createHolder(XMLReader reader, RecursionChecker chk)
      throws CircularReferenceException {
    if (reader instanceof org.apache.xerces.parsers.SAXParser)
      return createHolderXerces((org.apache.xerces.parsers.SAXParser)reader);
    if (reader instanceof javax.xml.parsers.SAXParser)
      return createHolder((javax.xml.parsers.SAXParser)reader, chk);
    chk.check(reader);
    return new Holder(reader);
  }

  private static Holder createHolder(javax.xml.parsers.SAXParser saxParser, RecursionChecker chk)
      throws CircularReferenceException {
    chk.check(saxParser);
    if (saxParser instanceof SAXParserImpl2) {
      final var inst = (SAXParserImpl2)saxParser;
      return createHolder(inst.getXMLReader(), chk);
    }

    if (saxParser instanceof SAXParserImpl) {
      final var inst = (SAXParserImpl)saxParser;
      return createHolder(inst.getXMLReader(), chk);
    }

    return new Holder(saxParser);
  }

  private static Holder createHolderXerces(org.apache.xerces.parsers.SAXParser saxParser) {
    //chk.check(saxParser);
    final var clazz = org.apache.xerces.parsers.XMLParser.class;
    try {
      Field _fConfiguration = clazz.getDeclaredField("fConfiguration");
      _fConfiguration.setAccessible(true);
      return new PCSHolder(saxParser, _fConfiguration);
    } catch (Exception e) {}
    return new Holder(saxParser);
  }
}
