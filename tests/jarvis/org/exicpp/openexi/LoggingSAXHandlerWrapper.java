//===- openexi/LoggingSAXHandlerWrapper.java ------------------------===//
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
import java.lang.System;
import java.util.ArrayList;
import java.util.HashMap;

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

import org.exicpp.util.ReflectionHelpers;

import javax.xml.transform.TransformerFactory;
import javax.xml.transform.sax.SAXTransformerFactory;

public class LoggingSAXHandlerWrapper extends DefaultHandler
    implements ContentHandler, LexicalHandler, DTDHandler, EntityResolver,
               DeclHandler, ErrorHandler {
  protected ContentHandler contentHandler;
  protected LexicalHandler lexicalHandler;
  protected DTDHandler dtdHandler;
  protected EntityResolver entityResolver;
  protected DeclHandler declHandler;
  protected ErrorHandler errorHandler;
  protected int elementCount = 0;

  public LoggingSAXHandlerWrapper(ContentHandler contentHandler) {
    // Wrap ContentHandler, and optionally wrap other SAX interfaces to allow
    // forwarding of all events
    this.contentHandler = contentHandler;
    if (contentHandler instanceof LexicalHandler)
      lexicalHandler = (LexicalHandler)contentHandler;
    if (contentHandler instanceof DTDHandler)
      dtdHandler = (DTDHandler)contentHandler;
    if (contentHandler instanceof EntityResolver)
      entityResolver = (EntityResolver)contentHandler;
    if (contentHandler instanceof DeclHandler)
      declHandler = (DeclHandler)contentHandler;
    if (contentHandler instanceof ErrorHandler)
      errorHandler = (ErrorHandler)contentHandler;
  }

  private void format(String format, Object...args) {
    if (elementCount != 0)
      System.out.format("%1$" + (elementCount * 2) + "s", "");
    System.out.format(format + "%n", args);
  }

  public void startElement(String uri, String localName, String qName,
                           Attributes atts) throws SAXException {
    format("SE: %s", qName);
    for (int I = 0; I < atts.getLength(); ++I) {
      format(" AT: %s=\"%s\"",
        atts.getQName(I),
        atts.getValue(I)
      );
    }
    // strip the root start element and forward all other elements
    if (elementCount > 0) {
      contentHandler.startElement(uri, localName, qName, atts);
    }
    elementCount++;
  }

  public void endElement(String uri, String localName, String qName)
      throws SAXException {
    // strip the root end element and forward all other elements
    elementCount--;
    format("EE: %s", qName);
    if (elementCount > 0) {
      contentHandler.endElement(uri, localName, qName);
    }
  }

  public void characters(char[] ch, int start, int length) throws SAXException {
    format("CH: { %s }", new String(ch, start, length));
    contentHandler.characters(ch, start, length);
  }

  public void endDocument() throws SAXException {
    format("}%n");
    contentHandler.endDocument();
  }

  public void endPrefixMapping(String prefix) throws SAXException {
    format("NS(end): xmlns:%s", prefix);
    contentHandler.endPrefixMapping(prefix);
  }

  public void ignorableWhitespace(char[] ch, int start, int length)
      throws SAXException {
    format("CH(ignored)");
    contentHandler.ignorableWhitespace(ch, start, length);
  }

  public void processingInstruction(String target, String data)
      throws SAXException {
    format("PI: %s %s", target, data);
    contentHandler.processingInstruction(target, data);
  }

  public void setDocumentLocator(Locator locator) {
    contentHandler.setDocumentLocator(locator);
  }

  public void skippedEntity(String name) throws SAXException {
    format("ER(skipped): %s", name);
    contentHandler.skippedEntity(name);
  }

  public void startDocument() throws SAXException {
    format("{");
    contentHandler.startDocument();
  }

  public void startPrefixMapping(String prefix, String uri)
      throws SAXException {
    format("NS: xmlns:%s=\"%s\"", prefix, uri);
    contentHandler.startPrefixMapping(prefix, uri);
  }

  public void comment(char[] ch, int start, int length) throws SAXException {
    format("CM: { %s }", new String(ch, start, length));
    if (lexicalHandler != null)
      lexicalHandler.comment(ch, start, length);
  }

  public void endCDATA() throws SAXException {
    format("]]>");
    if (lexicalHandler != null)
      lexicalHandler.endCDATA();
  }

  public void endDTD() throws SAXException {
    format("DT(end)");
    if (lexicalHandler != null)
      lexicalHandler.endDTD();
  }

  public void endEntity(String name) throws SAXException {
    format("ER(end): %s", name);
    if (lexicalHandler != null)
      lexicalHandler.endEntity(name);
  }

  public void startCDATA() throws SAXException {
    format("<![CDATA[");
    if (lexicalHandler != null)
      lexicalHandler.startCDATA();
  }

  public void startDTD(String name, String publicId, String systemId)
      throws SAXException {
    format("DT: %s %s %s",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    if (lexicalHandler != null)
      lexicalHandler.startDTD(name, publicId, systemId);
  }

  public void startEntity(String name) throws SAXException {
    format("ER: %s", name);
    if (lexicalHandler != null)
      lexicalHandler.startEntity(name);
  }

  public void notationDecl(String name, String publicId, String systemId)
      throws SAXException {
    format("DT: <!NOTATION %s %s %s>",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    if (dtdHandler != null)
      dtdHandler.notationDecl(name, publicId, systemId);
  }

  public void unparsedEntityDecl(String name, String publicId, String systemId,
                                 String notationName) throws SAXException {
    format("DT: <!ENTITY %s %s %s %s>",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?",
      notationName
    );
    if (dtdHandler != null)
      dtdHandler.unparsedEntityDecl(name, publicId, systemId, notationName);
  }

  public InputSource resolveEntity(String publicId, String systemId)
      throws SAXException, IOException {
    format("ER(resolve): %s %s",
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    if (entityResolver != null)
      return entityResolver.resolveEntity(publicId, systemId);
    else
      return null;
  }

  public void attributeDecl(String eName, String aName, String type,
                            String mode, String value) throws SAXException {
    format("DT: <!ATTLIST %s %s %s %s %s>", eName, aName, type, mode, value);
    if (declHandler != null)
      declHandler.attributeDecl(eName, aName, type, mode, value);
  }

  public void elementDecl(String name, String model) throws SAXException {
    format("DT: <!ELEMENT %s %s>", name, model);
    if (declHandler != null)
      declHandler.elementDecl(name, model);
  }

  public void externalEntityDecl(String name, String publicId, String systemId)
      throws SAXException {
    format("DT: <!ENTITY(x) %s %s %s>",
      name,
      publicId != null ? publicId : "?",
      systemId != null ? systemId : "?"
    );
    if (declHandler != null)
      declHandler.externalEntityDecl(name, publicId, systemId);
  }

  public void internalEntityDecl(String name, String value)
      throws SAXException {
    format("DT: <!ENTITY(i) %s %s>", name, value);
    if (declHandler != null)
      declHandler.internalEntityDecl(name, value);
  }

  public void error(SAXParseException exception) throws SAXException {
    if (errorHandler != null)
      errorHandler.error(exception);
    else
      System.err.println("[ERROR] " + exception.toString());
  }

  public void fatalError(SAXParseException exception) throws SAXException {
    if (errorHandler != null)
      errorHandler.fatalError(exception);
    else
      throw exception;
  }

  public void warning(SAXParseException exception) throws SAXException {
    if (errorHandler != null)
      errorHandler.warning(exception);
    else
      System.err.println("[WARN] " + exception.toString());
  }
}
