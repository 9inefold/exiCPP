//===- xerces/impl/XMLEntityManager2.java ---------------------------===//
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.Reader;
import java.io.StringReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Locale;
import java.util.Map;
import java.util.Stack;
import java.util.StringTokenizer;

import org.apache.xerces.impl.XMLEntityManager;
import org.apache.xerces.impl.io.ASCIIReader;
import org.apache.xerces.impl.io.Latin1Reader;
import org.apache.xerces.impl.io.UCSReader;
import org.apache.xerces.impl.io.UTF16Reader;
import org.apache.xerces.impl.io.UTF8Reader;
import org.apache.xerces.impl.msg.XMLMessageFormatter;
import org.apache.xerces.impl.validation.ValidationManager;
import org.apache.xerces.util.AugmentationsImpl;
import org.apache.xerces.util.EncodingMap;
import org.apache.xerces.util.HTTPInputSource;
import org.apache.xerces.util.SecurityManager;
import org.apache.xerces.util.SymbolTable;
import org.apache.xerces.util.URI;
import org.apache.xerces.util.XMLChar;
import org.apache.xerces.util.XMLEntityDescriptionImpl;
import org.apache.xerces.util.XMLResourceIdentifierImpl;
import org.apache.xerces.xni.Augmentations;
import org.apache.xerces.xni.XMLResourceIdentifier;
import org.apache.xerces.xni.XNIException;
import org.apache.xerces.xni.parser.XMLComponent;
import org.apache.xerces.xni.parser.XMLComponentManager;
import org.apache.xerces.xni.parser.XMLConfigurationException;
import org.apache.xerces.xni.parser.XMLEntityResolver;
import org.apache.xerces.xni.parser.XMLInputSource;

public class XMLEntityManager2 extends XMLEntityManager {
  protected final HashSet<String> fFilePaths = new HashSet<>();

  public XMLEntityManager2() {
    super();
  }

  public XMLEntityManager2(XMLEntityManager mgr) {
    super(mgr);
    //fValidation = mgr.fValidation;
    //fExternalGeneralEntities = mgr.fExternalGeneralEntities;
    //fExternalParameterEntities = mgr.fExternalParameterEntities;
    //fAllowJavaEncodings = mgr.fAllowJavaEncodings;
    //fWarnDuplicateEntityDef = mgr.fWarnDuplicateEntityDef;
    //fStrictURI = mgr.fStrictURI;
    //fSymbolTable = mgr.fSymbolTable;
    //fErrorReporter = mgr.fErrorReporter;
    //fEntityResolver = mgr.fEntityResolver;
    //fValidationManager = mgr.fValidationManager;
    //fBufferSize = mgr.fBufferSize;
    //fSecurityManager = mgr.fSecurityManager;
    //fStandalone = mgr.fStandalone;
    //fHasPEReferences = mgr.fHasPEReferences;
    //fInExternalSubset = mgr.fInExternalSubset;
    //fEntityHandler = mgr.fEntityHandler;
    //fEntityScanner = mgr.fEntityScanner;
    //fXML10EntityScanner = mgr.fXML10EntityScanner;
    //fXML11EntityScanner = mgr.fXML11EntityScanner;
    //fEntityExpansionLimit = mgr.fEntityExpansionLimit;
    //fEntityExpansionCount = mgr.fEntityExpansionCount;
    //fCurrentEntity = mgr.fCurrentEntity;
  }

  public void addSearchDirectory(String filePath) {
    if (filePath != null)
      fFilePaths.add(filePath);
  }

  public void resetSearchDirectories() {
    fFilePaths.clear();
  }

  /**
   * This method uses the passed-in XMLInputSource to make 
   * fCurrentEntity usable for reading.
   * @param name  name of the entity (XML is it's the document entity)
   * @param xmlInputSource    the input source, with sufficient information
   *      to begin scanning characters.
   * @param literal        True if this entity is started within a
   *                       literal value.
   * @param isExternal    whether this entity should be treated as an internal or external entity.
   * @throws IOException  if anything can't be read
   *  XNIException    If any parser-specific goes wrong.
   * @return the encoding of the new entity or null if a character stream was employed
   */
  public String setupCurrentEntity(String name, XMLInputSource xmlInputSource,
                                   boolean literal, boolean isExternal)
      throws IOException, XNIException, FileNotFoundException {
    String notFoundMsg = null;
    try {
      return super.setupCurrentEntity(
          name, xmlInputSource, literal, isExternal);
    } catch (FileNotFoundException e) {
      notFoundMsg = e.getMessage();
    }

    final String publicId = xmlInputSource.getPublicId();
    final String systemId = xmlInputSource.getSystemId();
    final String baseSystemId = xmlInputSource.getBaseSystemId();

    System.err.println("SYSTEM: " + systemId);
    if (fFilePaths.isEmpty())
      throw new FileNotFoundException(notFoundMsg);    

    // Try resolving with the provided directory
    var newXmlInputSource = new XMLInputSource(publicId, systemId, baseSystemId);
    for (String searchDir : fFilePaths) {
      newXmlInputSource.setSystemId(searchDir + "/" + systemId);
      System.err.println("  TRYING: " + newXmlInputSource.getSystemId());
      try {
        return super.setupCurrentEntity(
            name, newXmlInputSource, literal, isExternal);
      } catch (FileNotFoundException e) {
        // ...
      }
    }

    throw new FileNotFoundException(notFoundMsg);
  }
}
