//===- util/EvilDocumentHijacker.java -------------------------------===//
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
/// \file
/// A VERY evil utility class which modifies xerces parser internals.
///
//===----------------------------------------------------------------===//

package org.exicpp.util;

import java.io.*;
import java.lang.NoSuchFieldException;
import java.lang.System;
import java.lang.reflect.Field;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.HashMap;
import org.apache.xerces.parsers.EEAwareParserConfiguration;
import org.apache.xerces.parsers.XIncludeAwareParserConfiguration;
import org.apache.xerces.parsers.XML11Configuration;
import org.apache.xerces.xni.parser.XMLComponentManager;
import org.xml.sax.XMLReader;

/// In Xerces 2.12.2:
/// Let xerces mean org.apache.xerces
///
/// xerces.XMLReader is xerces.jaxp.SAXParserImpl$JAXPSAXParser
/// JAXPSAXParser instantiates super xerces.parsers.SAXParser
///
/// SAXParser either:
///   - loads xerces.xni.parser.XMLParserConfiguration
///   - or falls back to xerces.parsers.XIncludeAwareParserConfiguration
/// then passes that instance all the way up to super xerces.parsers.XMLParser,
/// XMLParser.fConfiguration is set to this value.
///
public class EvilDocumentHijacker {
  private static final String XERCES_NS = "org.apache.xerces";
  private static final String XERCES_PFX = "org.apache.xerces.";

  private static final String XMLREADER_NAME =
      XERCES_PFX + "jaxp.SAXParserImpl$JAXPSAXParser";
  private static final String XMLREADER_CANON_NAME =
      XERCES_PFX + "jaxp.SAXParserImpl.JAXPSAXParser";
  private static final String XMLREADER2_CANON_NAME =
      XERCES_PFX + "jaxp.SAXParserImpl2.JAXPSAXParser";

  private static final String XNI_PARSERCONFIGURATION =
      XERCES_PFX + "xni.parser.XMLParserConfiguration";
  private static final String XINCLUDE_PARSERCONFIGURATION =
      XERCES_PFX + "parsers.XIncludeAwareParserConfiguration";

  private static final int JAVA_VERSION;
  static { JAVA_VERSION = getJavaVersion(); }

  private enum ParserConfigurationKind { NOTFOUND, XINCLUDE, XNI }

  private static ParserConfigurationKind
  getRecognizedParserConfiguration(Class<?> clazz) {
    String name = clazz.getName();
    if (XINCLUDE_PARSERCONFIGURATION.equals(name))
      return ParserConfigurationKind.XINCLUDE;
    else if (XNI_PARSERCONFIGURATION.equals(name))
      return ParserConfigurationKind.XNI;
    // Fallback name
    try {
      Class<?> xniConfiguration = Class.forName(XNI_PARSERCONFIGURATION);
      if (xniConfiguration.isAssignableFrom(clazz))
        return ParserConfigurationKind.XNI;
    } catch (Exception e) {
    }
    return ParserConfigurationKind.NOTFOUND;
  }

  @SuppressWarnings("removal")
  private static String getSystemProperty(final String propName) {
    try {
      return AccessController.doPrivileged(new PrivilegedAction<String>() {
        public String run() { return System.getProperty(propName); }
      });
    } catch (Exception e) {
      return null;
    }
  }

  private static String getJavaVersionString() {
    String prop = getSystemProperty("java.vm.version");
    if (prop != null || prop.length() == 0)
      return prop;
    prop = getSystemProperty("java.runtime.version");
    if (prop != null || prop.length() == 0)
      return prop;
    return getSystemProperty("java.version");
  }

  static int getJavaVersion() {
    String version = getJavaVersionString();
    if (version == null || version.length() == 0)
      return 0;
    if (version.startsWith("1."))
      version = version.substring(2, 3);
    int dot = version.indexOf(".");
    if (dot != -1)
      version = version.substring(0, dot);
    return Integer.parseInt(version);
  }

  /// Returns whether the reader could be reconfigured.
  public static boolean reconfigureXMLReader(XMLReader reader) throws SecurityException {
    if (reader == null) {
      System.err.println("XMLReader is null!");
      return false;
    }

    Class<? extends XMLReader> clazz = reader.getClass();
    if (XMLREADER2_CANON_NAME.equals(clazz.getCanonicalName())) {
      // Already set to our custom type!
      return true;
    } else if (!XMLREADER_CANON_NAME.equals(clazz.getCanonicalName())) {
      System.err.println("Unknown XMLReader type: " + clazz.getName());
      return false;
    }

    if (JAVA_VERSION >= 12) {
      System.err.println("Unable to reconfigure in Java version " + JAVA_VERSION);
      return false;
    }

    Field _fConfiguration = null;
    try {
      _fConfiguration =
          ReflectionHelpers.locateMemberInSuperclasses(clazz, "fConfiguration");
      System.out.println(_fConfiguration.getDeclaringClass().getName());
    } catch (NoSuchFieldException e) {
      System.err.println(e.getMessage());
      ReflectionHelpers.printSuperclassFields(clazz);
      return false;
    }
    assert _fConfiguration != null;

    if (!XMLComponentManager.class.isAssignableFrom(_fConfiguration.getType())) {
      System.err.println("fConfiguration type does not extend XMLComponentManager?");
      return false;
    }

    _fConfiguration.setAccessible(true);
    @SuppressWarnings("unchecked")
    var fConfiguration =
        (XMLComponentManager)ReflectionHelpers.getObjectField(reader, _fConfiguration);

    var configSig = getRecognizedParserConfiguration(fConfiguration.getClass());
    if (configSig == ParserConfigurationKind.NOTFOUND) {
      System.err.println("Unknown fConfiguration type: " +
                         fConfiguration.getClass().getName());
      return false;
    }

    if (configSig == ParserConfigurationKind.XNI) {
      System.err.println("TODO ParserConfigurationKind.XNI: " +
                         fConfiguration.getClass().getName());
    }

    try {
      // TODO: Change final
      _fConfiguration.set(fConfiguration, new EEAwareParserConfiguration());
    } catch (Exception e) {
      System.err.println("Failed to set fConfiguration.");
      // e.printStackTrace(System.err);
      return false;
    }

    return true;

    // Class<?> configclazz = fConfiguration.getDeclaringClass();
    // System.out.format("fConfiguration in %s: %s%n",
    //                   configclazz.getName(), configtype.getName());

    // ReflectionHelpers.printClassFields(clazz);
    // ReflectionHelpers.printSuperclassFields(clazz);
    // ReflectionHelpers.printSuperclassMethods(clazz);
    // System.out.format("%s: %s%n", clazz.getName(), clazz.getCanonicalName());
  }
}
