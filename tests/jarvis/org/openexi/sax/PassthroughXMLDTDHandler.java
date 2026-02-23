//===- openexi/PassthroughXMLDTDHandler.java ------------------------===//
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

package org.openexi.sax;

import org.apache.xerces.xni.Augmentations;
import org.apache.xerces.xni.XMLDTDHandler;
import org.apache.xerces.xni.XMLLocator;
import org.apache.xerces.xni.XMLResourceIdentifier;
import org.apache.xerces.xni.XMLString;
import org.apache.xerces.xni.XNIException;
import org.apache.xerces.xni.parser.XMLDTDSource;

public final class PassthroughXMLDTDHandler implements XMLDTDHandler {

  /** Implementing handler */
  private final PartialXMLDTDHandler fDTDHandler;
  /** DTD source */
  protected XMLDTDSource fDTDSource = null;

  public PassthroughXMLDTDHandler(PartialXMLDTDHandler dtdHandler) {
    assert dtdHandler != null;
    fDTDHandler = dtdHandler;
  }

  /**
   * The start of the DTD.
   *
   * @param locator  The document locator, or null if the document
   *                 location cannot be reported during the parsing of 
   *                 the document DTD. However, it is <em>strongly</em>
   *                 recommended that a locator be supplied that can 
   *                 at least report the base system identifier of the
   *                 DTD.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void startDTD(XMLLocator locator, Augmentations augs) throws XNIException {
    fDTDHandler.startDTD(locator, augs);
  } // startDTD(XMLLocator)


  /**
   * The start of the DTD external subset.
   *
   * @param augmentations Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void startExternalSubset(XMLResourceIdentifier identifier, Augmentations augmentations) 
      throws XNIException {
    fDTDHandler.startExternalSubset(identifier, augmentations);
  } // startExternalSubset(Augmentations)

  /**
   * The end of the DTD external subset.
   *
   * @param augmentations Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void endExternalSubset(Augmentations augmentations) 
      throws XNIException {
    fDTDHandler.endExternalSubset(augmentations);
  } // endExternalSubset(Augmentations)

  /**
   * A comment.
   *
   * @param text The text in the comment.
   * @param augmentations Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by application to signal an error.
   */
  public void comment(XMLString text, Augmentations augs) throws XNIException {
    fDTDHandler.comment(text, augs);
  }

  /**
   * A processing instruction. Processing instructions consist of a
   * target name and, optionally, text data. The data is only meaningful
   * to the application.
   * <p>
   * Typically, a processing instruction's data will contain a series
   * of pseudo-attributes. These pseudo-attributes follow the form of
   * element attributes but are <strong>not</strong> parsed or presented
   * to the application as anything other than text. The application is
   * responsible for parsing the data.
   * 
   * @param target The target.
   * @param data   The data or null if none specified.
   * @param augs   Additional information that may include infoset augmentations
   *               
   * @exception XNIException
   *                   Thrown by handler to signal an error.
   */
  public void processingInstruction(String target, XMLString data, Augmentations augs)
      throws XNIException {
    fDTDHandler.processingInstruction(target, data, augs);
  } // processingInstruction(String, XMLString, Augmentations)

  /**
   * This method notifies the start of an entity.
   * <p>
   * <strong>Note:</strong> This method is not called for entity references
   * appearing as part of attribute values.
   * 
   * @param name     The name of the entity.
   * @param identifier The resource identifier.
   * @param encoding The auto-detected IANA encoding name of the entity
   *                 stream. This value will be null in those situations
   *                 where the entity encoding is not auto-detected (e.g.
   *                 internal entities or a document entity that is
   *                 parsed from a java.io.Reader).
   * @param augs     Additional information that may include infoset augmentations
   *                 
   * @exception XNIException Thrown by handler to signal an error.
   */
  public void startParameterEntity(String name, 
                                   XMLResourceIdentifier identifier,
                                   String encoding,
                                   Augmentations augs) throws XNIException {
    fDTDHandler.startParameterEntity(name, identifier, encoding, augs);
  } // startParameterEntity(String,XMLResourceIdentifier,String,Augmentations)

  /**
   * Notifies of the presence of a TextDecl line in an entity. If present,
   * this method will be called immediately following the startEntity call.
   * <p>
   * <strong>Note:</strong> This method is only called for external
   * parameter entities referenced in the DTD.
   *
   * @param version  The XML version, or null if not specified.
   * @param encoding The IANA encoding name of the entity.
   * @param augmentations Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void textDecl(String version, String encoding, Augmentations augs)
      throws XNIException {
    fDTDHandler.textDecl(version, encoding, augs);
  }

  /**
   * This method notifies the end of an entity.
   * <p>
   * <strong>Note:</strong> This method is not called for entity references
   * appearing as part of attribute values.
   * 
   * @param name   The name of the entity.
   * @param augs   Additional information that may include infoset augmentations
   *               
   * @exception XNIException
   *                   Thrown by handler to signal an error.
   */
  public void endParameterEntity(String name, Augmentations augs) 
      throws XNIException {
    fDTDHandler.endParameterEntity(name, augs);
  } // endParameterEntity(String,Augmentations)
  
  /**
   * Characters within an IGNORE conditional section.
   *
   * @param text The ignored text.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void ignoredCharacters(XMLString text, Augmentations augs) throws XNIException {
    fDTDHandler.ignoredCharacters(text, augs);
  } // ignoredCharacters(XMLString, Augmentations)

  /**
   * An element declaration.
   * 
   * @param name         The name of the element.
   * @param contentModel The element content model.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void elementDecl(String name, String contentModel, Augmentations augs)
      throws XNIException {
    fDTDHandler.elementDecl(name, contentModel, augs);
  } // elementDecl(String,String)

  /**
   * The start of an attribute list.
   * 
   * @param elementName The name of the element that this attribute
   *                    list is associated with.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void startAttlist(String elementName, Augmentations augs) throws XNIException {
    fDTDHandler.startAttlist(elementName, augs);
  } // startAttlist(String)

  /**
   * An attribute declaration.
   * 
   * @param elementName   The name of the element that this attribute
   *                      is associated with.
   * @param attributeName The name of the attribute.
   * @param type          The attribute type. This value will be one of
   *                      the following: "CDATA", "ENTITY", "ENTITIES",
   *                      "ENUMERATION", "ID", "IDREF", "IDREFS", 
   *                      "NMTOKEN", "NMTOKENS", or "NOTATION".
   * @param enumeration   If the type has the value "ENUMERATION" or
   *                      "NOTATION", this array holds the allowed attribute
   *                      values; otherwise, this array is null.
   * @param defaultType   The attribute default type. This value will be
   *                      one of the following: "#FIXED", "#IMPLIED",
   *                      "#REQUIRED", or null.
   * @param defaultValue  The attribute default value, or null if no
   *                      default value is specified.
   * @param nonNormalizedDefaultValue  The attribute default value with no normalization 
   *                      performed, or null if no default value is specified.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void attributeDecl(String elementName, String attributeName, 
                            String type, String[] enumeration, 
                            String defaultType, XMLString defaultValue, 
		                        XMLString nonNormalizedDefaultValue, Augmentations augs)
      throws XNIException {
    fDTDHandler.attributeDecl(elementName, attributeName, type, enumeration,
                              defaultType, defaultValue, nonNormalizedDefaultValue, augs);
  } // attributeDecl(String,String,String,String[],String,XMLString, XMLString, Augmentations)

  /**
   * The end of an attribute list.
   *
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void endAttlist(Augmentations augs) throws XNIException {
    fDTDHandler.endAttlist(augs);
  } // endAttlist()

  /**
   * An internal entity declaration.
   * 
   * @param name The name of the entity. Parameter entity names start with
   *             '%', whereas the name of a general entity is just the 
   *             entity name.
   * @param text The value of the entity.
   * @param nonNormalizedText The non-normalized value of the entity. This
   *             value contains the same sequence of characters that was in 
   *             the internal entity declaration, without any entity
   *             references expanded.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void internalEntityDecl(String name, XMLString text,
                                 XMLString nonNormalizedText, Augmentations augs) 
      throws XNIException {
    fDTDHandler.internalEntityDecl(name, text, nonNormalizedText, augs);
  } // internalEntityDecl(String,XMLString,XMLString)

  /**
   * An external entity declaration.
   * 
   * @param name     The name of the entity. Parameter entity names start
   *                 with '%', whereas the name of a general entity is just
   *                 the entity name.
   * @param identifier    An object containing all location information 
   *                      pertinent to this entity.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void externalEntityDecl(String name, XMLResourceIdentifier identifier,
                                 Augmentations augs) throws XNIException {
    fDTDHandler.externalEntityDecl(name, identifier, augs);
  } // externalEntityDecl(String,XMLResourceIdentifier, Augmentations)

  /**
   * An unparsed entity declaration.
   * 
   * @param name     The name of the entity.
   * @param identifier    An object containing all location information 
   *                      pertinent to this entity.
   * @param notation The name of the notation.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void unparsedEntityDecl(String name, XMLResourceIdentifier identifier,
                                 String notation, Augmentations augs) throws XNIException {
    fDTDHandler.unparsedEntityDecl(name, identifier, notation, augs);
  } // unparsedEntityDecl(String,XMLResourceIdentifier, String, Augmentations)

  /**
   * A notation declaration
   * 
   * @param name     The name of the notation.
   * @param identifier    An object containing all location information 
   *                      pertinent to this notation.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void notationDecl(String name, XMLResourceIdentifier identifier, 
  	                       Augmentations augs)
      throws XNIException {
    fDTDHandler.notationDecl(name, identifier, augs);
  } // notationDecl(String,XMLResourceIdentifier, Augmentations)

  /**
   * The start of a conditional section.
   * 
   * @param type The type of the conditional section. This value will
   *             either be CONDITIONAL_INCLUDE or CONDITIONAL_IGNORE.
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   *
   * @see #CONDITIONAL_INCLUDE
   * @see #CONDITIONAL_IGNORE
   */
  public void startConditional(short type, Augmentations augs) throws XNIException {
    fDTDHandler.startConditional(type, augs);
  } // startConditional(short)

  /**
   * The end of a conditional section.
   *
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void endConditional(Augmentations augs) throws XNIException {
    fDTDHandler.endConditional(augs);
  } // endConditional()

  /**
   * The end of the DTD.
   *
   * @param augs Additional information that may include infoset
   *                      augmentations.
   *
   * @throws XNIException Thrown by handler to signal an error.
   */
  public void endDTD(Augmentations augs) throws XNIException {
    fDTDHandler.endDTD(augs);
  } // endDTD()

  // set the source of this handler
  public void setDTDSource(XMLDTDSource source) {
    fDTDSource = source;
  }

  // return the source from which this handler derives its events
  public XMLDTDSource getDTDSource() {
    return fDTDSource;
  }

} // class PassthroughXMLDTDHandler()
