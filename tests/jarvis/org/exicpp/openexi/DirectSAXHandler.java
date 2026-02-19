//===- openexi/DirectSAXHandler.java --------------------------------===//
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

import java.io.IOException;
import java.io.StringWriter;
import java.lang.System;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.lang.reflect.Field;
import java.lang.reflect.InaccessibleObjectException;

import org.xml.sax.Attributes;
import org.xml.sax.ContentHandler;
import org.xml.sax.DTDHandler;
import org.xml.sax.EntityResolver;
import org.xml.sax.ErrorHandler;
import org.xml.sax.InputSource;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;
import org.xml.sax.SAXNotRecognizedException;
import org.xml.sax.SAXNotSupportedException;
import org.xml.sax.ext.DeclHandler;
import org.xml.sax.ext.LexicalHandler;
import org.xml.sax.helpers.DefaultHandler;

import org.exicpp.util.Log;
import org.exicpp.util.ReflectionHelpers;
import org.exicpp.util.XConstants;

import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.sax.SAXTransformerFactory;
import javax.xml.transform.sax.TransformerHandler;
//import com.sun.org.apache.xalan.internal.xsltc.trax.TransformerImpl;
//import com.sun.org.apache.xalan.internal.xsltc.trax.TransformerHandlerImpl;
//import com.sun.org.apache.xml.internal.serializer.ToUnknownStream;

public class DirectSAXHandler extends DefaultHandler
    implements ContentHandler, LexicalHandler, DTDHandler, EntityResolver,
               DeclHandler, ErrorHandler {
  /** Maps Prefix="URI" */
  public static class URIMapping {
    public String pfx = "";
    public String uri = "";

    public URIMapping() {}
    public URIMapping(String uri) { this("", uri); }
    public URIMapping(String pfx, String uri) {
      assert pfx != null && uri != null;
      this.pfx = pfx;
      this.uri = uri;
    }
  };

  /** The output writer. */
  protected StringWriter writer = null;

  /** The namespace mappings from Prefix="URI". */
  protected final HashMap<String, String> mappings;
  /** The stack of URI mappings */
  protected final ArrayList<ArrayList<URIMapping>> mappingStack;
  /** The URI mappings in the current scope */
  protected ArrayList<URIMapping> localMappings;

  /** Current element depth. */
  protected int elementCount = 0;

  /// Methods

  public DirectSAXHandler() {
    mappings = new HashMap<>();
    mappingStack = new ArrayList<>(32);
    localMappings = null;
  }

  private void format(String format, Object...args) {
    //if (Log.hasExtra()) {
    //  if (elementCount != 0)
    //    System.out.format("%1$" + (elementCount * 2) + "s", "");
    //  System.out.format(format + "%n", args);
    //}
  }

  public void setWriter(StringWriter writer) {
    this.writer = writer;
  }

  public void reset() {
    writer = null;
    mappings.clear();
    elementCount = 0;
  }

  //public void setDocumentLocator(Locator locator) {
  //  contentHandler.setDocumentLocator(locator);
  //}

  @Override
  public void startDocument() throws SAXException {
    if (writer == null)
      throw new SAXException("StringWriter not initialized!");
    format("{");
    //contentHandler.startDocument();
  }

  @Override
  public void endDocument() throws SAXException {
    format("}%n");
    //contentHandler.endDocument();
  }

  @Override
  public void startElement(String uri, String localName, String qName,
                           Attributes atts) throws SAXException {
    format("TODO SE: %s", qName);
    // Write element.
    writer.write("<");
    writer.write(qName);

    if (localMappings != null) {
      for (URIMapping mapping : localMappings) {
        writer.write(" xmlns");
        if (mapping.pfx.length() != 0) {
          writer.write(':');
          writer.write(mapping.pfx);
        }
        writer.write("=\"");
        writer.write(mapping.uri);
        writer.write("\"");
      }
    }

    for (int I = 0; I < atts.getLength(); ++I) {
      String atQName = atts.getQName(I);
      String atValue = atts.getValue(I);
      format(" AT: %s=\"%s\"", atQName, atValue);
      writer.write(' ');
      writer.write(atQName);
      writer.write("=\"");
      writer.write(atValue);
      writer.write("\"");
    }

    writer.write('>');
    // Handle other stuff
    pushPrefixMappings();
    elementCount++;
  }

  @Override
  public void endElement(String uri, String localName, String qName)
      throws SAXException {
    writer.write("</");
    writer.write(qName);
    writer.write('>');
    // Handle other stuff
    elementCount--;
    format("EE: %s", qName);
    popPrefixMappings();
  }

  private void addPrefixMapping(String pfx, String uri) {
    var globalMapping = mappings.get(pfx);
    if (globalMapping != null) {
      if (globalMapping.equals(uri))
        return;
    }

    if (localMappings == null)
      localMappings = new ArrayList<>(2);
    localMappings.addLast(new URIMapping(pfx, uri));
  }

  private void pushPrefixMappings() {
    if (localMappings == null || localMappings.size() == 0) {
      // Nothing to do.
      mappingStack.add(localMappings);
      return;
    }

    for (URIMapping mapping : localMappings)
      mappings.put(mapping.pfx, mapping.uri);

    mappingStack.addLast(localMappings);
    localMappings = null;
  }

  private void popPrefixMappings() {
    localMappings = mappingStack.removeLast();
    if (localMappings == null || localMappings.size() == 0)
      return;
    for (URIMapping mapping : localMappings)
      mappings.put(mapping.pfx, mapping.uri);
    localMappings.clear();
  }

  @Override
  public void startPrefixMapping(String prefix, String uri)
      throws SAXException {
    format("NS: xmlns:%s=\"%s\"", prefix, uri);
    addPrefixMapping(prefix, uri);
    //contentHandler.startPrefixMapping(prefix, uri);
  }

  @Override
  public void endPrefixMapping(String prefix) throws SAXException {
    format("NS(end): xmlns:%s", prefix);
    //contentHandler.endPrefixMapping(prefix);
  }

  @Override
  public void characters(char[] ch, int start, int length) throws SAXException {
    format("CH: { %s }", new String(ch, start, length));
    writer.write(ch, start, length);
  }

  @Override
  public void ignorableWhitespace(char[] ch, int start, int length)
      throws SAXException {
    format("CH(ignored)");
    writer.write(ch, start, length);
  }

  @Override
  public void startCDATA() throws SAXException {
    format("<![CDATA[");
    writer.write(XConstants.CDATA_START, 0, 9);
  }

  @Override
  public void endCDATA() throws SAXException {
    format("]]>");
    writer.write(XConstants.CDATA_END, 0, 3);
  }

  @Override
  public void processingInstruction(String target, String data)
      throws SAXException {
    format("PI: %s %s", target, data);
    writer.write("<?");
    writer.write(target);
    writer.write(' ');
    writer.write(data);
    writer.write("?>");
  }

  @Override
  public void startEntity(String name) throws SAXException {
    format("ER: %s", name);
    //if (lexicalHandler != null)
    //  lexicalHandler.startEntity(name);
    writer.write('&');
    writer.write(name);
    writer.write(';');
  }

  @Override
  public void endEntity(String name) throws SAXException {
    format("ER(end): %s", name);
    //if (lexicalHandler != null)
    //  lexicalHandler.endEntity(name);
  }

  @Override
  public void skippedEntity(String name) throws SAXException {
    format("ER(skipped): %s", name);
    writer.write('&');
    writer.write(name);
    writer.write(';');
  }

  @Override
  public void comment(char[] ch, int start, int length) throws SAXException {
    format("CM: { %s }", new String(ch, start, length));
    //if (lexicalHandler != null)
    //  lexicalHandler.comment(ch, start, length);
    writer.write("<!--");
    writer.write(ch, start, length);
    writer.write("-->");
  }

  @Override
  public void notationDecl(String name, String publicId, String systemId)
      throws SAXException {
    format("DT: <!NOTATION %s %s %s>",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    //if (dtdHandler != null)
    //  dtdHandler.notationDecl(name, publicId, systemId);
  }

  @Override
  public void unparsedEntityDecl(String name, String publicId, String systemId,
                                 String notationName) throws SAXException {
    format("DT: <!ENTITY %s %s %s %s>",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?",
      notationName
    );
    //if (dtdHandler != null)
    //  dtdHandler.unparsedEntityDecl(name, publicId, systemId, notationName);
  }

  @Override
  public InputSource resolveEntity(String publicId, String systemId)
      throws SAXException, IOException {
    format("ER(resolve): %s %s",
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    //if (entityResolver != null)
    //  return entityResolver.resolveEntity(publicId, systemId);
    //else
      return null;
  }

  @Override
  public void startDTD(String name, String publicId, String systemId) throws SAXException {
    format("DTD: %s, %s, %s", name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
  }

  @Override
  public void endDTD() {
    format("DTD(end)");
  }

  @Override
  public void attributeDecl(String eName, String aName, String type,
                            String mode, String value) throws SAXException {
    format("DT: <!ATTLIST %s %s %s %s %s>", eName, aName, type, mode, value);
    //if (declHandler != null)
    //  declHandler.attributeDecl(eName, aName, type, mode, value);
  }

  @Override
  public void elementDecl(String name, String model) throws SAXException {
    format("DT: <!ELEMENT %s %s>", name, model);
    //if (declHandler != null)
    //  declHandler.elementDecl(name, model);
  }

  @Override
  public void externalEntityDecl(String name, String publicId, String systemId)
      throws SAXException {
    format("DT: <!ENTITY(x) %s %s %s>",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    //if (declHandler != null)
    //  declHandler.externalEntityDecl(name, publicId, systemId);
  }

  @Override
  public void internalEntityDecl(String name, String value)
      throws SAXException {
    format("DT: <!ENTITY(i) %s %s>", name, value);
    //if (declHandler != null)
    //  declHandler.internalEntityDecl(name, value);
  }

  @Override
  public void fatalError(SAXParseException exception) throws SAXException {
    throw exception;
  }

  @Override
  public void error(SAXParseException exception) throws SAXException {
    Log.error("[ERROR] " + exception.toString());
  }

  @Override
  public void warning(SAXParseException exception) throws SAXException {
    Log.warn("[WARNING] " + exception.toString());
  }
}
