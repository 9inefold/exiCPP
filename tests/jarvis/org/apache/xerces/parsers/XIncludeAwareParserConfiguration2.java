/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.xerces.parsers;

import org.apache.xerces.impl.Constants;
import org.apache.xerces.impl.XML11DTDScannerImpl;
import org.apache.xerces.impl.XML11DocumentScannerImpl;
import org.apache.xerces.impl.XML11NSDocumentScannerImpl;
import org.apache.xerces.impl.XMLDTDScannerImpl;
import org.apache.xerces.impl.XMLDocumentScannerImpl;
import org.apache.xerces.impl.XMLEntityHandler;
import org.apache.xerces.impl.XMLEntityManager;
import org.apache.xerces.impl.XMLErrorReporter;
import org.apache.xerces.impl.XMLNSDocumentScannerImpl;
import org.apache.xerces.impl.XMLVersionDetector;
import org.apache.xerces.impl.dtd.XML11DTDProcessor;
import org.apache.xerces.impl.dtd.XML11DTDValidator;
import org.apache.xerces.impl.dtd.XML11NSDTDValidator;
import org.apache.xerces.impl.dtd.XMLDTDProcessor;
import org.apache.xerces.impl.dtd.XMLDTDValidator;
import org.apache.xerces.impl.dtd.XMLNSDTDValidator;
import org.apache.xerces.impl.msg.XMLMessageFormatter;
import org.apache.xerces.impl.xs.XMLSchemaValidator;
import org.apache.xerces.impl.xs.XSMessageFormatter;
import org.apache.xerces.xni.XMLDTDHandler;
import org.apache.xerces.xni.XMLDocumentHandler;
import org.apache.xerces.util.NamespaceSupport;
import org.apache.xerces.util.SymbolTable;
import org.apache.xerces.xinclude.XIncludeHandler;
import org.apache.xerces.xinclude.XIncludeNamespaceSupport;
import org.apache.xerces.xni.NamespaceContext;
import org.apache.xerces.xni.XMLDocumentHandler;
import org.apache.xerces.xni.grammars.XMLGrammarPool;
import org.apache.xerces.xni.parser.XMLComponent;
import org.apache.xerces.xni.parser.XMLComponentManager;
import org.apache.xerces.xni.parser.XMLConfigurationException;
import org.apache.xerces.xni.parser.XMLDTDScanner;
import org.apache.xerces.xni.parser.XMLDocumentScanner;
import org.apache.xerces.xni.parser.XMLDocumentSource;
import org.apache.xerces.xni.parser.XMLEntityResolver;
import org.apache.xerces.xni.parser.XMLErrorHandler;
import org.apache.xerces.xni.parser.XMLInputSource;

import org.apache.xerces.impl.XMLEEDocumentScanner;
import org.apache.xerces.impl.XMLEENSDocumentScanner;
import org.exicpp.util.XConstants;

/**
 * This class is the configuration used to parse XML 1.0 and XML 1.1 documents
 * and provides support for XInclude. This is the default Xerces configuration.
 *
 * @author Michael Glavassevich, IBM
 *
 * @version $Id: XIncludeAwareParserConfiguration2.java 987475 2010-08-20
 * 12:27:44Z mrglavas $
 */
public class XIncludeAwareParserConfiguration2 extends XML11Configuration {
  /** Feature identifier: notify built-in refereces. */
  private static final String NOTIFY_BUILTIN_REFS =
    Constants.XERCES_FEATURE_PREFIX + Constants.NOTIFY_BUILTIN_REFS_FEATURE;

  /**
   * Feature identifier: allow notation and unparsed entity events to be sent
   * out of order.
   */
  protected static final String ALLOW_UE_AND_NOTATION_EVENTS =
      Constants.SAX_FEATURE_PREFIX +
      Constants.ALLOW_DTD_EVENTS_AFTER_ENDDTD_FEATURE;

  /** Feature identifier: fixup base URIs. */
  protected static final String XINCLUDE_FIXUP_BASE_URIS =
      Constants.XERCES_FEATURE_PREFIX +
      Constants.XINCLUDE_FIXUP_BASE_URIS_FEATURE;

  /** Feature identifier: fixup language. */
  protected static final String XINCLUDE_FIXUP_LANGUAGE =
      Constants.XERCES_FEATURE_PREFIX +
      Constants.XINCLUDE_FIXUP_LANGUAGE_FEATURE;

  /** Feature identifier: XInclude processing */
  protected static final String XINCLUDE_FEATURE =
      Constants.XERCES_FEATURE_PREFIX + Constants.XINCLUDE_FEATURE;
  
  /** Feature identifier: Embedded escape sequences */
  protected static final String EMBED_ESCAPE_SEQUENCES =
      XConstants.EXICPP_FEATURE_PREFIX + XConstants.EMBED_ESCAPE_SEQUENCES;
  
  // Properties

  /** Property identifier: symbol table. */
  protected static final String SYMBOL_TABLE =
    Constants.XERCES_PROPERTY_PREFIX +
    Constants.SYMBOL_TABLE_PROPERTY;

  /** Property identifier: XML grammar pool. */
  protected static final String XMLGRAMMAR_POOL =
    Constants.XERCES_PROPERTY_PREFIX +
    Constants.XMLGRAMMAR_POOL_PROPERTY;
  
  /** Property identifier: XInclude handler. */
  protected static final String ESCAPE_HANDLER =
      XConstants.EXICPP_PROPERTY_PREFIX + XConstants.ESCAPE_HANDLER_PROPERTY;

  /** Property identifier: XInclude handler. */
  protected static final String XINCLUDE_HANDLER =
      Constants.XERCES_PROPERTY_PREFIX + Constants.XINCLUDE_HANDLER_PROPERTY;

  /** Property identifier: error reporter. */
  protected static final String NAMESPACE_CONTEXT =
      Constants.XERCES_PROPERTY_PREFIX + Constants.NAMESPACE_CONTEXT_PROPERTY;

  //
  // Components
  //

  /** Embedded Escape XMLDocumentScanner */
  protected XMLDocumentScanner fEECurrentScanner;

  /** Embedded Escape XMLNSDocumentScanner */
  protected XMLEENSDocumentScanner fEENamespaceScanner;

  /** Embedded Escape XMLDocumentScanner */
  protected XMLEEDocumentScanner fEENonNSScanner;

  /** XInclude handler. */
  protected XIncludeHandler fXIncludeHandler;

  /** Non-XInclude NamespaceContext. */
  protected NamespaceSupport fNonXIncludeNSContext;

  /** XInclude NamespaceContext. */
  protected XIncludeNamespaceSupport fXIncludeNSContext;

  /** Current NamespaceContext. */
  protected NamespaceContext fCurrentNSContext;

  /** Flag indicating whether XInclude processsing is enabled. */
  protected boolean fXIncludeEnabled = false;

  /** Flag indicating whether embedded escape sequences are enabled. */
  protected boolean fEmbeddedEscapesEnabled = false;

  /// Error states

  /** Flag indicating whether xml 1.1 embedded escapes error has been printed. */
  private boolean hpe_XML11EEE = false;

  /** Flag indicating whether xinclude embedded escapes error has been printed. */
  private boolean hpe_XIncludeEEE = false;

  /** Default constructor. */
  public XIncludeAwareParserConfiguration2() {
    this(null, null, null);
  } // <init>()

  /**
   * Constructs a parser configuration using the specified symbol table.
   *
   * @param symbolTable The symbol table to use.
   */
  public XIncludeAwareParserConfiguration2(SymbolTable symbolTable) {
    this(symbolTable, null, null);
  } // <init>(SymbolTable)

  /**
   * Constructs a parser configuration using the specified symbol table and
   * grammar pool.
   * <p>
   *
   * @param symbolTable The symbol table to use.
   * @param grammarPool The grammar pool to use.
   */
  public XIncludeAwareParserConfiguration2(SymbolTable symbolTable,
                                           XMLGrammarPool grammarPool) {
    this(symbolTable, grammarPool, null);
  } // <init>(SymbolTable,XMLGrammarPool)

  /**
   * Constructs a parser configuration using the specified symbol table,
   * grammar pool, and parent settings.
   * <p>
   *
   * @param symbolTable    The symbol table to use.
   * @param grammarPool    The grammar pool to use.
   * @param parentSettings The parent settings.
   */
  public XIncludeAwareParserConfiguration2(SymbolTable symbolTable,
                                           XMLGrammarPool grammarPool,
                                           XMLComponentManager parentSettings) {
    super(symbolTable, grammarPool, parentSettings);

    final String[] recognizedFeatures = {NOTIFY_BUILTIN_REFS,
                                         EMBED_ESCAPE_SEQUENCES,
                                         ALLOW_UE_AND_NOTATION_EVENTS,
                                         XINCLUDE_FIXUP_BASE_URIS,
                                         XINCLUDE_FIXUP_LANGUAGE};
    addRecognizedFeatures(recognizedFeatures);

    // add default recognized properties
    final String[] recognizedProperties = {SYMBOL_TABLE,
                                           XMLGRAMMAR_POOL,
                                           ESCAPE_HANDLER,
                                           XINCLUDE_HANDLER,
                                           NAMESPACE_CONTEXT};
    addRecognizedProperties(recognizedProperties);

    setFeature(EMBED_ESCAPE_SEQUENCES, false);
    setFeature(ALLOW_UE_AND_NOTATION_EVENTS, true);
    setFeature(XINCLUDE_FIXUP_BASE_URIS, true);
    setFeature(XINCLUDE_FIXUP_LANGUAGE, true);

    fEENamespaceScanner = new XMLEENSDocumentScanner();
    fEECurrentScanner = fEENamespaceScanner;
    setProperty(ESCAPE_HANDLER, fEENamespaceScanner);
    addComponent((XMLComponent) fEENamespaceScanner);

    fNonXIncludeNSContext = new NamespaceSupport();
    fCurrentNSContext = fNonXIncludeNSContext;
    setProperty(NAMESPACE_CONTEXT, fNonXIncludeNSContext);
  }

  public static XIncludeAwareParserConfiguration2 newInstance() {
    return newInstance(null, null);
  }

  public static XIncludeAwareParserConfiguration2 newInstance(SymbolTable symbolTable) {
    return newInstance(symbolTable, null);
  }

  public static XIncludeAwareParserConfiguration2 newInstance(SymbolTable symbolTable, XMLGrammarPool grammarPool) {
    var config = new XIncludeAwareParserConfiguration2(symbolTable, grammarPool, null);
    config.setFeature(NOTIFY_BUILTIN_REFS, true);
    if (symbolTable != null)
      config.setProperty(SYMBOL_TABLE, symbolTable);
    if (grammarPool != null)
      config.setProperty(XMLGRAMMAR_POOL, grammarPool);
    return config;
  }

  protected void configureXInclude() {
    if (fXIncludeEnabled) {
      // If the XInclude handler was not in the pipeline insert it.
      if (fXIncludeHandler == null) {
        fXIncludeHandler = new XIncludeHandler();
        // add XInclude component
        setProperty(XINCLUDE_HANDLER, fXIncludeHandler);
        addCommonComponent(fXIncludeHandler);
        fXIncludeHandler.reset(this);
      }
      // Setup NamespaceContext
      if (fCurrentNSContext != fXIncludeNSContext) {
        if (fXIncludeNSContext == null) {
          fXIncludeNSContext = new XIncludeNamespaceSupport();
        }
        fCurrentNSContext = fXIncludeNSContext;
        setProperty(NAMESPACE_CONTEXT, fXIncludeNSContext);
      }
    } else {
      // Setup NamespaceContext
      if (fCurrentNSContext != fNonXIncludeNSContext) {
        fCurrentNSContext = fNonXIncludeNSContext;
        setProperty(NAMESPACE_CONTEXT, fNonXIncludeNSContext);
      }
    }
  }

  /** Configures the pipeline. */
  protected void configurePipeline() {
    if (!fEmbeddedEscapesEnabled)
      super.configurePipeline();
    else
      configurePipeline0();
    
    configureXInclude();
    if (fXIncludeEnabled) {
      // configure DTD pipeline
      fDTDScanner.setDTDHandler(fDTDProcessor);
      fDTDProcessor.setDTDSource(fDTDScanner);
      fDTDProcessor.setDTDHandler(fXIncludeHandler);
      fXIncludeHandler.setDTDSource(fDTDProcessor);
      fXIncludeHandler.setDTDHandler(fDTDHandler);
      if (fDTDHandler != null) {
        fDTDHandler.setDTDSource(fXIncludeHandler);
      }

      // configure XML document pipeline: insert after DTDValidator and
      // before XML Schema validator
      XMLDocumentSource prev = null;
      if (fFeatures.get(XMLSCHEMA_VALIDATION) == Boolean.TRUE) {
        // we don't have to worry about fSchemaValidator being null, since
        // super.configurePipeline() instantiated it if the feature was set
        prev = fSchemaValidator.getDocumentSource();
      }
      // Otherwise, insert after the last component in the pipeline
      else {
        prev = fLastComponent;
        fLastComponent = fXIncludeHandler;
      }

      XMLDocumentHandler next = prev.getDocumentHandler();
      prev.setDocumentHandler(fXIncludeHandler);
      fXIncludeHandler.setDocumentSource(prev);
      if (next != null) {
        fXIncludeHandler.setDocumentHandler(next);
        next.setDocumentSource(fXIncludeHandler);
      }
    }
  } // configurePipeline()

  protected void configureXML11Pipeline() {
    super.configureXML11Pipeline();
    if (fEmbeddedEscapesEnabled) {
      if (!hpe_XML11EEE) {
        System.err.format("Feature %s has not been implemented for xml 1.1%n",
                          XConstants.EMBED_ESCAPE_SEQUENCES);
        hpe_XML11EEE = true;
      }
    }

    configureXInclude();
    if (fXIncludeEnabled) {
      // configure XML 1.1. DTD pipeline
      fXML11DTDScanner.setDTDHandler(fXML11DTDProcessor);
      fXML11DTDProcessor.setDTDSource(fXML11DTDScanner);
      fXML11DTDProcessor.setDTDHandler(fXIncludeHandler);
      fXIncludeHandler.setDTDSource(fXML11DTDProcessor);
      fXIncludeHandler.setDTDHandler(fDTDHandler);
      if (fDTDHandler != null) {
        fDTDHandler.setDTDSource(fXIncludeHandler);
      }

      // configure XML document pipeline: insert after DTDValidator and
      // before XML Schema validator
      XMLDocumentSource prev = null;
      if (fFeatures.get(XMLSCHEMA_VALIDATION) == Boolean.TRUE) {
        // we don't have to worry about fSchemaValidator being null, since
        // super.configurePipeline() instantiated it if the feature was set
        prev = fSchemaValidator.getDocumentSource();
      }
      // Otherwise, insert after the last component in the pipeline
      else {
        prev = fLastComponent;
        fLastComponent = fXIncludeHandler;
      }

      XMLDocumentHandler next = prev.getDocumentHandler();
      prev.setDocumentHandler(fXIncludeHandler);
      fXIncludeHandler.setDocumentSource(prev);
      if (next != null) {
        fXIncludeHandler.setDocumentHandler(next);
        next.setDocumentSource(fXIncludeHandler);
      }
    }
  } // configureXML11Pipeline()

  /** Configures the pipeline. */
  protected void configurePipeline0() {
    assert fEmbeddedEscapesEnabled;
    if (fCurrentDVFactory != fDatatypeValidatorFactory) {
      fCurrentDVFactory = fDatatypeValidatorFactory;
      // use XML 1.0 datatype library
      setProperty(DATATYPE_VALIDATOR_FACTORY, fCurrentDVFactory);
    }

    // setup DTD pipeline
    if (fCurrentDTDScanner != fDTDScanner) {
      fCurrentDTDScanner = fDTDScanner;
      setProperty(DTD_SCANNER, fCurrentDTDScanner);
      setProperty(DTD_PROCESSOR, fDTDProcessor);
    }
    fDTDScanner.setDTDHandler(fDTDProcessor);
    fDTDProcessor.setDTDSource(fDTDScanner);
    fDTDProcessor.setDTDHandler(fDTDHandler);
    if (fDTDHandler != null) {
      fDTDHandler.setDTDSource(fDTDProcessor);
    }

    fDTDScanner.setDTDContentModelHandler(fDTDProcessor);
    fDTDProcessor.setDTDContentModelSource(fDTDScanner);
    fDTDProcessor.setDTDContentModelHandler(fDTDContentModelHandler);
    if (fDTDContentModelHandler != null) {
      fDTDContentModelHandler.setDTDContentModelSource(fDTDProcessor);
    }

    // setup document pipeline
    if (fFeatures.get(NAMESPACES) == Boolean.TRUE) {
      if (fEECurrentScanner != fEENamespaceScanner) {
        fEECurrentScanner = fEENamespaceScanner;
        setProperty(ESCAPE_HANDLER, fEENamespaceScanner);
      }
      if (fCurrentScanner != fEENamespaceScanner) {
        fCurrentScanner = fEENamespaceScanner;
        setProperty(DOCUMENT_SCANNER, fEENamespaceScanner);
        setProperty(DTD_VALIDATOR, fDTDValidator);
      }
      fEENamespaceScanner.setDTDValidator(fDTDValidator);
      fEENamespaceScanner.setDocumentHandler(fDTDValidator);
      fDTDValidator.setDocumentSource(fEENamespaceScanner);
      fDTDValidator.setDocumentHandler(fDocumentHandler);
      if (fDocumentHandler != null) {
        fDocumentHandler.setDocumentSource(fDTDValidator);
      }
      fLastComponent = fDTDValidator;
    } else {
      // create components
      if (fEENonNSScanner == null) {
        // TODO: Handle creating fNonNSScanner?
        fEENonNSScanner = new XMLEEDocumentScanner();
        // add components
        addComponent((XMLComponent)fEENonNSScanner);
      }
      if (fNonNSDTDValidator == null) {
        fNonNSDTDValidator = new XMLDTDValidator();
        // add components
        addComponent((XMLComponent)fNonNSDTDValidator);
      }
      if (fEECurrentScanner != fEENonNSScanner) {
        fEECurrentScanner = fEENonNSScanner;
        setProperty(ESCAPE_HANDLER, fEENonNSScanner);
      }
      if (fCurrentScanner != fEENonNSScanner) {
        fCurrentScanner = fEENonNSScanner;
        setProperty(ESCAPE_HANDLER, fEENonNSScanner);
        setProperty(DOCUMENT_SCANNER, fEENonNSScanner);
        setProperty(DTD_VALIDATOR, fNonNSDTDValidator);
      }

      fEENonNSScanner.setDocumentHandler(fNonNSDTDValidator);
      fNonNSDTDValidator.setDocumentSource(fEENonNSScanner);
      fNonNSDTDValidator.setDocumentHandler(fDocumentHandler);
      if (fDocumentHandler != null) {
        fDocumentHandler.setDocumentSource(fNonNSDTDValidator);
      }
      fLastComponent = fNonNSDTDValidator;
    }

    // add XML Schema validator if needed
    if (fFeatures.get(XMLSCHEMA_VALIDATION) == Boolean.TRUE) {
      // If schema validator was not in the pipeline insert it.
      if (fSchemaValidator == null) {
        fSchemaValidator = new XMLSchemaValidator();
        // add schema component
        setProperty(SCHEMA_VALIDATOR, fSchemaValidator);
        addCommonComponent(fSchemaValidator);
        fSchemaValidator.reset(this);
        // add schema message formatter
        if (fErrorReporter.getMessageFormatter(
                XSMessageFormatter.SCHEMA_DOMAIN) == null) {
          XSMessageFormatter xmft = new XSMessageFormatter();
          fErrorReporter.putMessageFormatter(XSMessageFormatter.SCHEMA_DOMAIN,
                                             xmft);
        }
      }
      fLastComponent.setDocumentHandler(fSchemaValidator);
      fSchemaValidator.setDocumentSource(fLastComponent);
      fSchemaValidator.setDocumentHandler(fDocumentHandler);
      if (fDocumentHandler != null) {
        fDocumentHandler.setDocumentSource(fSchemaValidator);
      }
      fLastComponent = fSchemaValidator;
    }
  } // configurePipeline0()

  public boolean getFeature(String featureId) throws XMLConfigurationException {
    if (featureId.equals(PARSER_SETTINGS)) {
      return fConfigUpdated;
    } else if (featureId.equals(XINCLUDE_FEATURE)) {
      return fXIncludeEnabled;
    } else if (featureId.equals(EMBED_ESCAPE_SEQUENCES)) {
      return fEmbeddedEscapesEnabled;
    }
    return super.getFeature0(featureId);

  } // getFeature(String):boolean

  public void setFeature(String featureId, boolean state)
      throws XMLConfigurationException {
    if (featureId.equals(XINCLUDE_FEATURE)) {
      fXIncludeEnabled = state;
      fConfigUpdated = true;
      return;
    } else if (featureId.equals(EMBED_ESCAPE_SEQUENCES)) {
      fEmbeddedEscapesEnabled = state;
      fConfigUpdated = true;
      return;
    }
    super.setFeature(featureId, state);
  }
}
