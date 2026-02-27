//===- openexi/EXIReader2.java --------------------------------------===//
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

// To get access to private shi (will replace with reflection?)
package org.openexi.sax;

import java.io.IOException;
import java.io.InputStream;

import org.xml.sax.DTDHandler;
import org.xml.sax.EntityResolver;
import org.xml.sax.ErrorHandler;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXNotRecognizedException;
import org.xml.sax.SAXNotSupportedException;
import org.xml.sax.XMLReader;
import org.xml.sax.ext.LexicalHandler;

import org.exicpp.sax.DTDBodyHandler;
import org.openexi.schema.Characters;
import org.openexi.proc.common.AlignmentType;
import org.openexi.proc.common.EventDescription;
import org.openexi.proc.common.EXIOptionsException;
import org.openexi.proc.common.QName;
import org.openexi.proc.common.XmlUriConst;
import org.openexi.proc.events.EXIEventDTD;
import org.openexi.proc.events.EXIEventNS;
import org.openexi.proc.events.EXIEventSchemaNil;
import org.openexi.proc.events.EXIEventSchemaType;
import org.openexi.proc.io.Scanner;
import org.openexi.proc.EXISchemaResolver;

/**
 * EXIReader2 implements the SAX XMLReader to provide a convenient and 
 * familiar interface for decoding an EXI stream.
 */
public final class EXIReader2 extends ReaderSupport implements XMLReader {

  private boolean m_hasLexicalHandler;
  private LexicalHandler m_lexicalHandler;

  private static final Characters CHARACTERS_TRUE;
  private static final Characters CHARACTERS_FALSE;
  static {
    CHARACTERS_TRUE = new Characters("true".toCharArray(), 0, "true".length(), false);
    CHARACTERS_FALSE = new Characters("false".toCharArray(), 0, "false".length(), false);
  }
  
  public EXIReader2() {
    super();
    m_hasLexicalHandler = false;
    m_lexicalHandler = null;
  }
  
  ///////////////////////////////////////////////////////////////////////////
  // XMLReader APIs
  ///////////////////////////////////////////////////////////////////////////
  
  /**
   * Set a SAX DTDHandler to receive notation events and
   * unparsed entity declaration events.
   * @param dtdHandler SAX DTDHandler
   * Not yet implemented.
   * @y.exclude
   */
  public final void setDTDHandler(DTDHandler dtdHandler) {
    //m_dtdHandler = dtdHandler;
  }
  /**
   * Get the SAX DTDHandler used to receive notation events
   * and unparsed entity declaration events.
   * @y.exclude
   */
  public final DTDHandler getDTDHandler() {
    //return m_dtdHandler;
    return null;
  }
  /**
   * @y.exclude
   */
  public void setEntityResolver(EntityResolver resolver) {
    // nothing to do.
  }
  /**
   * @y.exclude
   */
  public final EntityResolver getEntityResolver() {
    // nothing to do.
    return null;
  }
  /**
   * @y.exclude
   */
  public final void setErrorHandler(ErrorHandler errorHandler) {
    // nothing to do.
  }
  /**
   * @y.exclude
   */
  public final ErrorHandler getErrorHandler() {
    // nothing to do.
    return null;
  }
  /**
   * This method wraps the friendlier setLexicalHandler method to provide 
   * syntax familiar to experienced SAX programmers. The only property 
   * supported is: <pre>http://xml.org/sax/properties/lexical-handler</pre>
   * @param name must equal "http://xml.org/sax/properties/lexical-handler"
   * @param value an org.xml.sax.ext.LexicalHandler object
   */

  public final void setProperty(String name, Object value) throws SAXNotRecognizedException {
    if ("http://xml.org/sax/properties/lexical-handler".equals(name)) {
      setLexicalHandler((LexicalHandler)value);
      return;
    }
    else if ("http://xml.org/sax/properties/declaration-handler".equals(name)) {
      // REVISIT: add support for declaration handler.
      return;
    }
    throw new SAXNotRecognizedException("Property '" + name + "' is not recognized.");
  }
  /**
   * Use to retrieve the name of the lexical handler, currently the only
   * property recognized by this class. Pass the String
   * "http://xml.org/sax/properties/lexical-handler" as the name.
   * @return String name of the lexical handler
   */
  public final Object getProperty(String name) throws SAXNotRecognizedException {
    if ("http://xml.org/sax/properties/lexical-handler".equals(name)) {
      return this.m_lexicalHandler;
    }
    else if ("http://xml.org/sax/properties/declaration-handler".equals(name)) {
      // REVISIT: add support for declaration handler.
      return null;
    }
    throw new SAXNotRecognizedException("Property '" + name + "' is not recognized.");
  }
  /**
   * Set features for the SAX parser. The only supported arguments are <pre>
   * EXIReader2.setFeature("http://xml.org/sax/features/namespaces", true);</pre> and <pre>
   * EXIReader2.setFeature("http://xml.org/sax/features/namespace-prefixes", false);</pre>
   */
  public final void setFeature(String name, boolean value)
    throws SAXNotRecognizedException, SAXNotSupportedException {
    if ("http://xml.org/sax/features/namespaces".equals(name)) {
      if (!value) {
        throw new SAXNotSupportedException("");
      }
      return;
    }
    else if ("http://xml.org/sax/features/namespace-prefixes".equals(name)) {
      if (value) {
        throw new SAXNotSupportedException("");
      }
      return;
    }
    throw new SAXNotRecognizedException("Feature '" + name + "' is not recognized.");
  }
  /**
   * Get features for the SAX parser.
   * @return <i>true</i> if the feature is "http://xml.org/sax/features/namespaces"
   * and <i>false</i> if the feature is "http://xml.org/sax/features/namespace-prefixes"
   */ 
  public final boolean getFeature(String name) throws SAXNotRecognizedException {
    if ("http://xml.org/sax/features/namespaces".equals(name)) {
      return true;
    }
    else if ("http://xml.org/sax/features/namespace-prefixes".equals(name)) {
      return false;
    }
    throw new SAXNotRecognizedException("Feature '" + name + "' is not recognized.");
  }
  /**
   * Reads the EXI input source and restores the XML stream per the schema and 
   * grammar options.
   * Not yet implemented.
   * @y.exclude
   */
  public void parse(InputSource input) throws IOException, SAXException {
    final InputStream inputStream;
    if ((inputStream = input.getByteStream()) != null) {
      parse(inputStream);
    }
    else
      throw new SAXNotSupportedException("");
  }
  /**
   * Reads the EXI input stream from a file on the system
   * and restores the XML stream per the schema and 
   * grammar options.
   * Not yet implemented.
   * @y.exclude
   */
  public void parse(String systemId) throws SAXException {
    throw new SAXNotSupportedException("");
  }

  ///////////////////////////////////////////////////////////////////////////
  // Methods to configure EXIDecoder
  ///////////////////////////////////////////////////////////////////////////

  /**
   * Set the bit alignment style used to compile the EXI input stream.
   * @param alignmentType {@link org.openexi.proc.common.AlignmentType} 
   * @throws EXIOptionsException
   */
  public final void setAlignmentType(AlignmentType alignmentType) throws EXIOptionsException {
    m_decoder.setAlignmentType(alignmentType);
  }

  /** 
   * Set to true if the EXI input stream is an XML fragment (a non-compliant
   * XML document with multiple root elements).
   * @param isFragment true if the EXI input stream is an XML fragment.
   */
  public final void setFragment(boolean isFragment) {
    m_decoder.setFragment(isFragment);
  }

  /**
   * Set to true if the EXI input stream was compiled with the Preserve Lexical
   * Values set to true. The original strings, rather than logical XML
   * equivalents, are restored in the XML output stream.
   * @param preserveLexicalValues set to true if the EXI input stream was compiled with 
   * Preserve Lexical Values set to true.
   * @throws EXIOptionsException
   */
  public final void setPreserveLexicalValues(boolean preserveLexicalValues) throws EXIOptionsException {
    m_decoder.setPreserveLexicalValues(preserveLexicalValues);
  }

  /** 
   * Set the EXISchemaResolver to retrieve the schema needed to decode the 
   * current EXI stream.
   * @param schemaResolver {@link org.openexi.proc.EXISchemaResolver}
   */
  public final void setEXISchemaResolver(EXISchemaResolver schemaResolver) {
    m_decoder.setEXISchemaResolver(schemaResolver);
  }
  
  /**
   * Set a datatype representation map.
   * @param dtrm a sequence of pairs of datatype qname and datatype representation qname
   * @param n_bindings the number of qname pairs
   */
  public final void setDatatypeRepresentationMap(QName[] dtrm, int n_bindings) throws EXIOptionsException {
    m_decoder.setDatatypeRepresentationMap(dtrm, n_bindings);
  }

  /**
   * Set the size, in number of values, of the information that will be 
   * processed as a chunk of the entire EXI stream. Reducing the block size 
   * can improve performance for devices with limited dynamic memory. 
   * Default is 1,000,000 items (not 1MB, but 1,000,000 complete Attribute 
   * and Element values). Block size is only used when the EXI stream is
   * encoded with EXI-compression.
   * @param blockSize number of values in each processing block. Default is 1,000,000.
   * @throws EXIOptionsException
   */
  public final void setBlockSize(int blockSize) throws EXIOptionsException {
    m_decoder.setBlockSize(blockSize);
  }
  /**
   * Set the maximum length of a string that will be stored for reuse in the
   * String Table. By default, there is no maximum length. However, in data
   * sets that have long, unique strings of information, you can improve
   * performance by limiting the size to the length of strings that are more
   * likely to appear more than once.
   * @param valueMaxLength maximum length of entries in the String Table. 
   */
  public final void setValueMaxLength(int valueMaxLength) {
    m_decoder.setValueMaxLength(valueMaxLength);
  }
  /**
   * Set the maximum number of values in the String Table. By default, there
   * is no limit. If the target device has limited dynamic memory, limiting 
   * the number of entries in the String Table can improve performance and
   * reduce the likelihood that you will exceed memory capacity.
   * @param valuePartitionCapacity maximum number of entries in the String Table
   */
  public final void setValuePartitionCapacity(int valuePartitionCapacity) {
    m_decoder.setValuePartitionCapacity(valuePartitionCapacity);
  }

  ///////////////////////////////////////////////////////////////////////////
  // 
  ///////////////////////////////////////////////////////////////////////////

  /**
   * Set a SAX lexical handler to receive SAX lexical events.
   * @param lexicalHandler SAX lexical handler
   */
  public void setLexicalHandler(LexicalHandler lexicalHandler) {
    m_hasLexicalHandler = (m_lexicalHandler = lexicalHandler) != null;
  }
  
  /**
   * Parse the input stream and restore the XML stream per the schema and
   * grammar options.
   * @param inputStream an encoded EXI stream
   * @throws IOException
   * @throws SAXException
   */
  private void parse(InputStream inputStream) throws IOException, SAXException {
    reset();
    final Scanner scanner = processHeader(inputStream);

    EventDescription exiEvent;
    while ((exiEvent = scanner.nextEvent()) != null) {
      final Characters characterSequence;
      switch (exiEvent.getEventKind()) {
        case EventDescription.EVENT_SD:
          m_contentHandler.startDocument();
          break;
        case EventDescription.EVENT_ED:
          m_contentHandler.endDocument();
          break;
        case EventDescription.EVENT_SE: 
        	doElement(exiEvent, scanner, 0);
          break;
        case EventDescription.EVENT_CM:
          if (m_hasLexicalHandler) {
            characterSequence = exiEvent.getCharacters();
            m_lexicalHandler.comment(characterSequence.characters, characterSequence.startIndex, characterSequence.length);
          }
          break;
        case EventDescription.EVENT_PI:
          m_contentHandler.processingInstruction(exiEvent.getName(), exiEvent.getCharacters().makeString());
          break;
        case EventDescription.EVENT_DTD: 
          if (m_hasLexicalHandler)
            doDTD((EXIEventDTD)exiEvent);
          break;
        default:
          break;
      }
    }
  }

  private void doDTD(EXIEventDTD eventDTD) throws SAXException {
    assert m_hasLexicalHandler;
    m_lexicalHandler.startDTD(eventDTD.getName(), eventDTD.getPublicId(), eventDTD.getSystemId());
    if (m_lexicalHandler instanceof DTDBodyHandler dtdHandler) {
      final Characters dtdCharacters = eventDTD.getCharacters();
      if (dtdCharacters.length != 0)
        dtdHandler.dtdBody(dtdCharacters.makeString());
    }
    m_lexicalHandler.endDTD();
  }

  private void doXsiNil(EventDescription exiEvent) throws SAXException {
    String attrQualifiedName, attrPrefix;
    if (m_preserveNS) {
      attrPrefix = exiEvent.getPrefix();
      assert attrPrefix.length() != 0;
      stringBuilder.setLength(0);
      attrQualifiedName = stringBuilder.append(attrPrefix).append(":nil").toString();
    }
    else {
      final int uriId = exiEvent.getURIId();
      assert uriId < m_n_prefixes;
      stringBuilder.setLength(0);
      attrQualifiedName = stringBuilder.append(m_prefixesColon[uriId]).append("nil").toString();
    }
    addAttribute(XmlUriConst.W3C_2001_XMLSCHEMA_INSTANCE_URI, "nil", attrQualifiedName, "", m_preserveLexicalValues ? 
        exiEvent.getCharacters() : ((EXIEventSchemaNil)exiEvent).isNilled() ? CHARACTERS_TRUE :  CHARACTERS_FALSE);
  }
  
  private boolean doXsiType(EventDescription exiEvent) throws SAXException {
  	boolean namespaceDeclAdded = false;
    final String attrQualifiedName, attrPrefix;
    if (m_preserveNS) {
      attrPrefix = exiEvent.getPrefix();
      assert attrPrefix.length() != 0;
      stringBuilder.setLength(0);
      attrQualifiedName = stringBuilder.append(attrPrefix).append(":type").toString();
    }
    else {
      final int uriId = exiEvent.getURIId();
      assert uriId < m_n_prefixes;
      stringBuilder.setLength(0);
      attrQualifiedName = stringBuilder.append(m_prefixesColon[uriId]).append("type").toString();
    }
    final EXIEventSchemaType eventSchemaType = (EXIEventSchemaType)exiEvent;
    final String typeQualifiedName;
    if (m_preserveLexicalValues) {
      typeQualifiedName = eventSchemaType.getCharacters().makeString();
    }
    else {
      final String typeName = eventSchemaType.getTypeName();
      final String typePrefix;
      if (m_preserveNS) {
        typePrefix = eventSchemaType.getTypePrefix();
        if (typePrefix.length() != 0) {
          stringBuilder.setLength(0);
          typeQualifiedName = stringBuilder.append(typePrefix).append(':').append(typeName).toString();
        }
        else
          typeQualifiedName = typeName;
      }
      else {
        // REVISIT: use eventSchemaType.getTypeURIId()
        final String typeUri = eventSchemaType.getTypeURI();
        if (typeUri.length() != 0) {
          int i;
          for (i = m_n_namespaceDeclarations - 1; i > -1; i--)  {
            if (typeUri.equals(m_namespaceDeclarationsLocus[i << 1 | 1])) {
              break;
            }
          }                  
          if (i != -1) {
            typePrefix = m_namespaceDeclarationsLocus[i << 1];
          }
          else {
            if (m_n_namespaceDeclarations < PREFIXES.length)
              typePrefix = PREFIXES[m_n_namespaceDeclarations];
            else { 
              stringBuilder.setLength(0);
              typePrefix = stringBuilder.append('p').append(m_n_namespaceDeclarations).toString();
            }
            m_contentHandler.startPrefixMapping(typePrefix, typeUri);
            pushNamespaceDeclaration(typePrefix, typeUri);
            namespaceDeclAdded = true;
          }
          stringBuilder.setLength(0);
          typeQualifiedName = stringBuilder.append(typePrefix).append(':').append(typeName).toString();
        }
        else 
          typeQualifiedName = typeName;
      }
    }
    final char[] typeQualifiedNameChars = typeQualifiedName.toCharArray();
    addAttribute(XmlUriConst.W3C_2001_XMLSCHEMA_INSTANCE_URI, "type", attrQualifiedName, "", new Characters(typeQualifiedNameChars, 0, typeQualifiedNameChars.length, false));
    return namespaceDeclAdded;
  }

  private void doElement(EventDescription exiEvent, Scanner scanner, final int depth) throws IOException, SAXException {
    final String elementURI;
    final String elementLocalName;
    final String elementQualifiedName;
    int n_namespaceDeclarations = 0;
  	
    String prefix;

    m_attrLength = 0;
    elementURI = exiEvent.getURI();
    elementLocalName = exiEvent.getName();
    if (m_preserveNS) {
      prefix = exiEvent.getPrefix();
      while ((exiEvent = scanner.nextEvent()) != null && exiEvent.getEventKind() == EventDescription.EVENT_NS) {
        final String _prefix;
        final String nsUri =  exiEvent.getURI();
        final String nsPrefix = exiEvent.getPrefix();
        m_contentHandler.startPrefixMapping(nsPrefix, nsUri);
        pushNamespaceDeclaration(nsPrefix, nsUri);
        ++n_namespaceDeclarations;
        _prefix = ((EXIEventNS)exiEvent).getLocalElementNs() ? nsPrefix : null;
        if (_prefix != null)
          prefix = _prefix;
      }
      if (prefix.length() != 0) {
        stringBuilder.setLength(0);
        elementQualifiedName = stringBuilder.append(prefix).append(':').append(elementLocalName).toString();
      }
      else {
        elementQualifiedName = elementLocalName;
      }
    }
    else {
      if (depth == 0) {
        for (int i = 1; i < m_n_prefixes; i++)
          m_contentHandler.startPrefixMapping(m_prefixes[i], m_uris[i]);
      }
      final int uriId = exiEvent.getURIId();
      if (uriId < m_n_prefixes) {
      	final int nameId = exiEvent.getNameId();
      	if (nameId < m_n_qualifiedNames[uriId])
      	  elementQualifiedName = m_qualifiedNames[uriId][nameId];
      	else {
          stringBuilder.setLength(0);
          elementQualifiedName = stringBuilder.append(m_prefixesColon[uriId]).append(elementLocalName).toString();
      	}
      }
      else {
        int i;
        if (elementURI != "") {
          for (i = m_n_namespaceDeclarations - 1; i > -1; i--)  {
            if (elementURI == m_namespaceDeclarationsLocus[i << 1 | 1])
              break;
          }
          if (i > -1)
            prefix = m_namespaceDeclarationsLocus[i << 1];
          else {
            if (m_n_namespaceDeclarations < PREFIXES.length)
              prefix = PREFIXES[m_n_namespaceDeclarations];
            else {
              stringBuilder.setLength(0);
              prefix = stringBuilder.append('p').append(m_n_namespaceDeclarations).toString();
            }
            m_contentHandler.startPrefixMapping(prefix, elementURI);
            pushNamespaceDeclaration(prefix, elementURI);
            ++n_namespaceDeclarations;            
          }
          if (prefix.length() != 0) {
            stringBuilder.setLength(0);
            elementQualifiedName = stringBuilder.append(prefix).append(':').append(elementLocalName).toString();
          }
          else
            elementQualifiedName = elementLocalName;
        }
        else {
          for (i = m_n_namespaceDeclarations - 1; i > -1; i--)  {
            // look for a namespace declaration for prefix ""
            if (m_namespaceDeclarationsLocus[i << 1].length() == 0) {
              if (m_namespaceDeclarationsLocus[i << 1 | 1].length() != 0) { // i.e. it was xmlns="..."
                m_contentHandler.startPrefixMapping("", ""); // reclaim the prefix "" for the uri ""
                pushNamespaceDeclaration("", "");
                ++n_namespaceDeclarations;            
              }
              break;
            }
          }
          elementQualifiedName = elementLocalName;
        }
      }
      exiEvent = scanner.nextEvent();
    }
    if (exiEvent.getEventKind() == EventDescription.EVENT_TP) {
      if (doXsiType(exiEvent))
    		++n_namespaceDeclarations;
      exiEvent = scanner.nextEvent();
    }
    if (exiEvent.getEventKind() == EventDescription.EVENT_NL) {
      doXsiNil(exiEvent);
      exiEvent = scanner.nextEvent();
    }
    while (exiEvent.getEventKind() == EventDescription.EVENT_AT) {
    	if (doAttribute(exiEvent, scanner))
    		++n_namespaceDeclarations;
      exiEvent = scanner.nextEvent();
    }
    m_contentHandler.startElement(elementURI, elementLocalName, elementQualifiedName, this);
    
    do {
    	switch (exiEvent.getEventKind()) {
	      case EventDescription.EVENT_SE: 
	      	doElement(exiEvent, scanner, depth + 1);
	      	break;
        case EventDescription.EVENT_EE:
	        m_contentHandler.endElement(elementURI, elementLocalName, elementQualifiedName);
	        if (!m_preserveNS && depth == 0) {
	          for (int i = 1; i < m_n_prefixes; i++)
	            m_contentHandler.endPrefixMapping(m_prefixes[i]);
	        }
	        final int n_prefixes = n_namespaceDeclarations;
	        for (int i = 0; i < n_prefixes; i++) {
	          m_contentHandler.endPrefixMapping(m_namespaceDeclarationsLocus[--m_n_namespaceDeclarations << 1]);
	        }
	      	return;
        case EventDescription.EVENT_CH:
	        final Characters characterSequence;
	        characterSequence = exiEvent.getCharacters();
	        m_contentHandler.characters(characterSequence.characters, characterSequence.startIndex, characterSequence.length);
	        break;
        case EventDescription.EVENT_CM:
          if (m_hasLexicalHandler) {
            characterSequence = exiEvent.getCharacters();
            m_lexicalHandler.comment(characterSequence.characters, characterSequence.startIndex, characterSequence.length);
          }
          break;
        case EventDescription.EVENT_PI:
          m_contentHandler.processingInstruction(exiEvent.getName(), exiEvent.getCharacters().makeString());
          break;
        case EventDescription.EVENT_ER:
	        m_contentHandler.skippedEntity(exiEvent.getName());
	        break;
	      case EventDescription.EVENT_NL:
	      case EventDescription.EVENT_TP:
	      case EventDescription.EVENT_AT:
	        assert false;
	        break;
    	}
      exiEvent = scanner.nextEvent();
    }
    while (true);
  }

}
