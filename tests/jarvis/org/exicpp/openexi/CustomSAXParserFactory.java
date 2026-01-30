//===- openexi/CustomSAXParserFactory.java --------------------------===//
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

package org.exicpp.openexi;

import java.io.PrintStream;
import java.lang.StringBuilder;
import java.lang.System;
//import java.lang.reflect.*;
import java.util.*;
import org.xml.sax.*;
import javax.xml.parsers.*;
import javax.xml.transform.sax.SAXTransformerFactory;

public class CustomSAXParserFactory extends SAXParserFactory {
  private CustomSAXParserFactory prototypeParser = null;
  private HashMap<String, Boolean> features;

  public CustomSAXParserFactory() {
    super();
  }

  static public void printTypeOfParser() throws ParserConfigurationException,
                                                SAXException {
    SAXParserFactory f = SAXParserFactory.newInstance();
    System.out.println("SAXParserFactory: " + f.getClass().getName());
    SAXParser p = f.newSAXParser();
    System.out.println("SAXParser: " + p.getClass().getName());
    XMLReader x = p.getXMLReader();
    System.out.println("XMLReader: " + x.getClass().getName());
  }

  public SAXParser newSAXParser() throws ParserConfigurationException {
    try {
      SAXParserFactory f = getPrototype();
      return f.newSAXParser();
      //return CustomSAXParserFactory.newInstance(features);
    } catch (SAXException se) {
      // Translate to ParserConfigurationException
      throw new ParserConfigurationException(se.getMessage());
    }
  }

  public void setFeature(String name, boolean value)
      throws ParserConfigurationException, SAXNotRecognizedException,
             SAXNotSupportedException {
    
    // First, let's see if it's a valid call
    getPrototype().setFeature(name, value);

    // If not, exception was thrown: so we are good now:
    if (features == null) {
      // Let's retain the ordering as well
      features = new LinkedHashMap<>();
    }

    features.put(name, value ? Boolean.TRUE : Boolean.FALSE);
  }

  public boolean getFeature(String name) throws ParserConfigurationException,
                                                SAXNotRecognizedException,
                                                SAXNotSupportedException {
    return getPrototype().getFeature(name);
  }

  public void listFeatures() {
    listFeatures(System.out);
  }

  public void listFeatures(PrintStream strm) {
    strm.println(formatFeatures());
  }

  ////////////////////////////////////////////////////////////////////////
  // Private Methods

  private String formatFeatures() {
    StringBuilder sb = new StringBuilder();
    sb.append("features {\n");
    for (Map.Entry<String, Boolean> entry : features.entrySet()) {
      String key = entry.getKey();
      Boolean value = entry.getValue();
      sb.append("  ");
      sb.append(key);
      sb.append(": ");
      sb.append(value.toString());
      sb.append("\n");
    }
    sb.append("}");
    return sb.toString();
  }

  private CustomSAXParserFactory getPrototype() {
    if (prototypeParser == null) {
      prototypeParser = new CustomSAXParserFactory();
    }
    return prototypeParser;
  }
}
